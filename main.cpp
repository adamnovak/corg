// main.cpp: Main file for core graph merger

#include <iostream>
#include <fstream>
#include <getopt.h>

#include "ekg/vg/vg.hpp"
#include "ekg/vg/index.hpp"

#include "embeddedGraph.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

/**
 * Get the leader for a pinch segment: either the first segment in its block, or
 * the segment itself if it has no block.
 */
stPinchSegment* getLeader(stPinchSegment* segment) {
    // See if the segment is in a block
    auto block = stPinchSegment_getBlock(segment);
    
    // Get the leader segment: first in the block, or this segment if no block
    auto leader = block ? stPinchBlock_getFirst(block) : segment;
    
    return leader;
}

/*
 * Return false if a segment is not in a block or is forward in its block, and
 * true otherwise. Converts from pinch graph orientations to vg orientations.
 */
bool getOrientation(stPinchSegment* segment) {

    auto block = stPinchSegment_getBlock(segment);
    
    return block ? !stPinchSegment_getBlockOrientation(segment) : false;

}

/**
 * Create a VG grpah from a pinch thread set.
 */
vg::VG pinchToVG(stPinchThreadSet* threadSet, std::map<int64_t, std::string>& threadSequences) {
    // Make an empty graph
    vg::VG graph;
    
    // Remember what nodes have been created for what segments. Only the first
    // segment in a block (the "leader") gets a node. Segments without blocks
    // are also themselves leaders and get nodes.
    std::map<stPinchSegment*, vg::Node*> nodeForLeader;
    
    std::cerr << "Making pinch graph into vg graph with " << threadSequences.size() << " relevant threads" << std::endl;
    
    // This is the cleverest way to loop over Benedict's iterators.
    auto segmentIterator = stPinchThreadSet_getSegmentIt(threadSet);
    while(auto segment = stPinchThreadSetSegmentIt_getNext(&segmentIterator)) {
        // For every segment, we need to make a VG node for it or its block (if
        // it has one).
        
#ifdef debug
        std::cerr << "Found segment " << segment << std::endl;
#endif
        
        // See if the segment is in a block
        auto block = stPinchSegment_getBlock(segment);
        
        // Get the leader segment: first in the block, or this segment if no block
        auto leader = getLeader(segment);
        
        if(nodeForLeader.count(leader)) {
            // A node has already been made for this block.
            continue;
        }
        
        // Otherwise, we need the sequence
        std::string sequence;
        
        if(block) {
            // Get the sequence by scanning through the block for the first sequence
            // that isn't all Ns, if any.
            auto segmentIterator = stPinchBlock_getSegmentIterator(block);
            while(auto sequenceSegment = stPinchBlockIt_getNext(&segmentIterator)) {
                if(!threadSequences.count(stPinchSegment_getName(sequenceSegment))) {
                    // This segment is part of a staple. Pass it up
                    continue;
                }
                
                // Go get the sequence of the thread, and clip out the part relevant to this segment.
                sequence = threadSequences.at(stPinchSegment_getName(sequenceSegment)).substr(
                    stPinchSegment_getStart(sequenceSegment), stPinchSegment_getLength(sequenceSegment));
                    
                // If necessary, flip the segment around
                if(getOrientation(sequenceSegment)) {
                    sequence = vg::reverse_complement(sequence);
                }
                
                if(std::count(sequence.begin(), sequence.end(), 'N') +
                    std::count(sequence.begin(), sequence.end(), 'n') < sequence.size()) {\
                    
                    // The sequence has some non-N characters
                    // If it's not all Ns, break
                    break;
                }
                
                // Otherwise try the next segment
            }
        } else {
            // Just pull the sequence from the lone segment
            sequence = threadSequences.at(stPinchSegment_getName(segment)).substr(
                stPinchSegment_getStart(segment), stPinchSegment_getLength(segment));
                
            // It doesn't need to flip, since it can't be backwards in a block
        }
        
            
        // Make a node in the graph to represent the block
        vg::Node* node = graph.create_node(sequence);
        
        // Remember it
        nodeForLeader[leader] = node;
#ifdef debug
        std::cerr << "Made node: " << pb2json(*node) << std::endl;
#endif
            
    }
    
    // Now go through the segments again and wire them up.
    segmentIterator = stPinchThreadSet_getSegmentIt(threadSet);
    while(auto segment = stPinchThreadSetSegmentIt_getNext(&segmentIterator)) {
        // See if the segment is in a block
        auto block = stPinchSegment_getBlock(segment);
        
        // Get the leader segment: first in the block, or this segment if no block
        auto leader = getLeader(segment);
        
        // We know we have a node already
        auto node = nodeForLeader.at(leader);
        
        // What orientation is this node in for the purposes of this edge
        // TODO: ought to always be false if the segment isn't in a block. Is this true?
        auto orientation = getOrientation(segment);
#ifdef debug
        std::cerr << "Revisited segment: " << segment << " for node " << node->id() <<
            " in orientation " << (orientation ? "reverse" : "forward") << std::endl;
#endif
        
        // Look at the segment 5' of here. We know it's not a staple and
        // thus has a vg node.
        auto prevSegment = stPinchSegment_get5Prime(segment);
        
        if(prevSegment) {
            // Get the node IDs and orientations
            auto prevNode = nodeForLeader.at(getLeader(prevSegment));
            auto prevOrientation = getOrientation(prevSegment);
#ifdef debug
            std::cerr << "Found prev node " << prevNode->id() << " in orientation " << 
                (prevOrientation ? "reverse" : "forward") << std::endl;
#endif
            
            // Make an edge
            vg::Edge prevEdge;
            prevEdge.set_from(prevNode->id());
            prevEdge.set_from_start(prevOrientation);
            prevEdge.set_to(node->id());
            prevEdge.set_to_end(orientation);
            
            // Add it in. vg::VG deduplicates for us
            graph.add_edge(prevEdge);
#ifdef debug
            std::cerr << "Made edge: " << pb2json(prevEdge) << std::endl;
#endif
        }
        
        // Now do the same thing for the 3' side
        auto nextSegment = stPinchSegment_get3Prime(segment);
        
        if(nextSegment) {
            // Get the node IDs and orientations
            auto nextNode = nodeForLeader.at(getLeader(nextSegment));
            auto nextOrientation = getOrientation(nextSegment);
#ifdef debug
            std::cerr << "Found next node " << nextNode->id() << " in orientation " << 
                (nextOrientation ? "reverse" : "forward") << std::endl;
#endif
            
            // Make an edge
            vg::Edge nextEdge;
            nextEdge.set_from(node->id());
            nextEdge.set_from_start(orientation);
            nextEdge.set_to(nextNode->id());
            nextEdge.set_to_end(nextOrientation);
            
            // Add it in. vg::VG deduplicates for us
            graph.add_edge(nextEdge);
#ifdef debug
            std::cerr << "Made edge: " << pb2json(nextEdge) << std::endl;
#endif
        }
    }
    
    // Spit out the graph.
    return graph;

}

void help_main(char** argv) {
    std::cerr << "usage: " << argv[0] << " [options] VGFILE VGFILE" << std::endl
        << "Compute the core graph from two graphs, and print it to standard "
        << "output in vg format." << std::endl
        << "The core graph is constructed by merging the two graphs together "
        << "along paths with the same name in both graphs. These paths must be "
        << "of the same length (which is checked) and spell out identical "
        << "sequences (which is not yet checked) for this tool to work "
        << "correctly." << std::endl << std::endl
        << "If -k is specified, the provided graphs must be indexed."
        << std::endl
        << "options:" << std::endl
        << "    -h, --help          print this help message" << std::endl
        << "    -k, --kmer-size N   join graphs on mutually unique kmers of size N" << std::endl
        << "    -e, --edge-max N    exclude k-paths which have N or more choice points" << std::endl
        << "    -o, --kmers-only    merge only on kmers, not on shared paths" << std::endl
        << "    -t, --threads N     number of threads to use" << std::endl;
}

int main(int argc, char** argv) {
    
    if(argc == 1) {
        // Print the help
        help_main(argv);
        return 1;
    }
    
    size_t kmerSize = 0;
    size_t edgeMax = 0;
    
    // Should we only merge on kmers and skip paths?
    bool kmersOnly = false;
    
    optind = 1; // Start at first real argument
    bool optionsRemaining = true;
    while(optionsRemaining) {
        static struct option longOptions[] = {
            {"kmer-size", required_argument, 0, 'k'},
            {"edge-max", required_argument, 0, 'e'},
            {"kmers-only", no_argument, 0, 'o'},
            {"threads", required_argument, 0, 't'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        int optionIndex = 0;

        switch(getopt_long(argc, argv, "k:e:t:h", longOptions, &optionIndex)) {
        // Option value is in global optarg
        case -1:
            optionsRemaining = false;
            break;
        case 'k': // Set the kmer size
            kmerSize = atol(optarg);
            break;
        case 'e': // Set the edge max parameter for kmer enumeration
            edgeMax = atol(optarg);
            break;
        case 'o': // Only merge on kmers
            kmersOnly = true;
            break;
        case 't': // Set the openmp threads
            omp_set_num_threads(atoi(optarg));
            break;
        case 'h': // When the user asks for help
        case '?': // When we get options we can't parse
            help_main(argv);
            exit(1);
            break;
        default:
            // TODO: keep track of the option
            std::cerr << "Illegal option" << std::endl;
            exit(1);
        }
    }
    
    if(argc - optind < 2) {
        // We don't have two positional arguments
        // Print the help
        help_main(argv);
        return 1;
    }
    
    if(kmersOnly && kmerSize == 0) {
        // We need a kmer size to use kmers
        throw std::runtime_error("Can't merge only on kmers with no kmer size");
    }
    
    // Pull out the VG file names
    std::string vgFile1 = argv[optind++];
    std::string vgFile2 = argv[optind++];
    
    // Guess index names (TODO: add options)
    std::string indexDir1 = vgFile1 + ".index";
    std::string indexDir2 = vgFile2 + ".index";
    
    // Open the files
    std::ifstream vgStream1(vgFile1);
    if(!vgStream1.good()) {
        std::cerr << "Could not read " << vgFile1 << std::endl;
        exit(1);
    }
    
    std::ifstream vgStream2(vgFile2);
    if(!vgStream2.good()) {
        std::cerr << "Could not read " << vgFile2 << std::endl;
        exit(1);
    }
    
    // We may have indexes. We need to use pointers because destructing an index
    // that was never opened segfaults. TODO: fix vg
    vg::Index* index1 = nullptr;
    vg::Index* index2 = nullptr;
    
    if(kmerSize) {
        // Only go looking for indexes if we want to merge on kmers.
        index1 = new vg::Index();
        index1->open_read_only(indexDir1);
        index2 = new vg::Index();
        index2->open_read_only(indexDir2);
    }
    
    
    // Load up the first VG file
    vg::VG vg1(vgStream1);
    // And the second
    vg::VG vg2(vgStream2);
    
    
    // Make a way to track IDs
    int64_t nextId = 1;
    std::function<int64_t(void)> getId = [&]() {
        return nextId++;
    };
    
    // Make a thread set
    auto threadSet = stPinchThreadSet_construct();
    
    // Make a place to keep track of the thread sequences.
    // This will only contain sequences for threads that aren't staples.
    // TODO: should this be by pointer instead?
    std::map<int64_t, std::string> threadSequences;
    
    // Add in each vg graph to the thread set
    coregraph::EmbeddedGraph embedding1(vg1, threadSet, threadSequences, getId, vgFile1);
    coregraph::EmbeddedGraph embedding2(vg2, threadSet, threadSequences, getId, vgFile2);
    
    if(!kmersOnly) {
        // We want to merge on shared paths in addition to kmers
    
        // Complain if any of the graphs is not completely covered by paths
        if(!embedding1.isCoveredByPaths()) {
            std::cerr << "WARNING: " << embedding1.getName() << " contains nodes with no paths!" << std::endl;
        }
        if(!embedding2.isCoveredByPaths()) {
            std::cerr << "WARNING: " << embedding2.getName() << " contains nodes with no paths!" << std::endl;
        }
        
        // Trace the paths and merge the embedded graphs.
        std::cerr << "Pinching graphs on shared paths..." << std::endl;
        embedding1.pinchWith(embedding2);
    }
    
    if(kmerSize > 0) {
        // Merge on kmers that are unique in both graphs.
        std::cerr << "Pinching graphs on shared " << kmerSize << "-mers..." << std::endl;
        embedding1.pinchOnKmers(*index1, embedding2, *index2, kmerSize, edgeMax);
    }
    
    // Fix trivial joins so we don't produce more vg nodes than we really need to.
    stPinchThreadSet_joinTrivialBoundaries(threadSet);
    
    // Make another vg graph from the thread set
    vg::VG core = pinchToVG(threadSet, threadSequences);
    
    // Spit it out to standard output
    core.serialize_to_ostream(std::cout);
    
    // Tear everything down. TODO: can we somehow run this destruction function
    // after all our other, potentially depending locals are destructed?
    stPinchThreadSet_destruct(threadSet);
    
    return 0;
}


