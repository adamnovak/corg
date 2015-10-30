// main.cpp: Main file for core graph merger

#include <iostream>
#include <fstream>
#include <getopt.h>

#include "ekg/vg/vg.hpp"

#include "embeddedGraph.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

/**
 * Create a VG grpah from a pinch thread set.
 */
vg::VG pinchToVG(stPinchThreadSet* threadSet, std::map<int64_t, std::string>& threadSequences) {
    // Make an empty graph
    vg::VG graph;
    
    // Remember what nodes have been created for what blocks
    std::map<stPinchBlock*, vg::Node*> nodeForBlock;
    
    std::cerr << "Making pinch graph into vg graph with " << threadSequences.size() << " relevant threads" << std::endl;
    
    // This is the cleverest way to loop over Benedict's iterators.
    auto blockIterator = stPinchThreadSet_getBlockIt(threadSet);
    while(auto block = stPinchThreadSetBlockIt_getNext(&blockIterator)) {
        // For every block of merged segments, we need to make a VG node.
        
        std::cerr << "Found block " << block << std::endl;
        
        // Get the sequence by scanning through for the first sequence that
        // isn't all Ns, if any.
        auto segmentIterator = stPinchBlock_getSegmentIterator(block);
        while(auto segment = stPinchBlockIt_getNext(&segmentIterator)) {
            if(!threadSequences.count(stPinchSegment_getName(segment))) {
                // This segment is part of a staple. Pass it up
                continue;
            }
            
            // Go get the sequence of the thread, and clip out the part relevant to this segment.
            std::string sequence = threadSequences.at(stPinchSegment_getName(segment)).substr(
                stPinchSegment_getStart(segment), stPinchSegment_getLength(segment));
                
            // If necessary, flip the segment around
            if(stPinchSegment_getBlockOrientation(segment)) {
                sequence = vg::reverse_complement(sequence);
            }
            
            // Make a node in the graph to represent the block
            vg::Node* node = graph.create_node(sequence);
            
            // Remember it
            nodeForBlock[block] = node;
            
            std::cerr << "Made node: " << pb2json(*node) << std::endl;
            
            // We don't need to see any more segments for this block
            break;
        }
    }
    
    // Now go through the blocks again and wire them up.
    blockIterator = stPinchThreadSet_getBlockIt(threadSet);
    while(auto block = stPinchThreadSetBlockIt_getNext(&blockIterator)) {
        // For every block of merged segments, we have a vg node already
        // TODO: There can't be any free-floating staples.
        auto node = nodeForBlock.at(block);
        
        std::cerr << "Revisited block: " << block << std::endl;
        
        // Get the sequence by scanning through for the first sequence that
        // isn't all Ns, if any.
        auto segmentIterator = stPinchBlock_getSegmentIterator(block);
        while(auto segment = stPinchBlockIt_getNext(&segmentIterator)) {
            if(!threadSequences.count(stPinchSegment_getName(segment))) {
                // This segment is part of a staple. Pass it up
                continue;
            }
            
            // What orientation is this node in for the purposes of this edge
            auto orientation = stPinchSegment_getBlockOrientation(segment);
            
            // Look at the segment 5' of here. We know it's not a staple and
            // thus has a vg node.
            auto prevSegment = stPinchSegment_get5Prime(segment);
            
            // Get the node IDs and orientations
            auto prevNode = nodeForBlock.at(stPinchSegment_getBlock(prevSegment));
            auto prevOrientation = stPinchSegment_getBlockOrientation(prevSegment);
            
            // Make an edge
            vg::Edge prevEdge;
            prevEdge.set_from(prevNode->id());
            prevEdge.set_from_start(prevOrientation);
            prevEdge.set_to(node->id());
            prevEdge.set_to_end(orientation);
            
            // Add it in. vg::VG deduplicates for us
            graph.add_edge(prevEdge);
            
            std::cerr << "Made edge: " << pb2json(prevEdge) << std::endl;
            
            // Now do the same thing for the 3' side
            auto nextSegment = stPinchSegment_get3Prime(segment);
            
            // Get the node IDs and orientations
            auto nextNode = nodeForBlock.at(stPinchSegment_getBlock(nextSegment));
            auto nextOrientation = stPinchSegment_getBlockOrientation(nextSegment);
            
            // Make an edge
            vg::Edge nextEdge;
            nextEdge.set_from(node->id());
            nextEdge.set_from_start(orientation);
            nextEdge.set_to(nextNode->id());
            nextEdge.set_to_end(nextOrientation);
            
            // Add it in. vg::VG deduplicates for us
            graph.add_edge(nextEdge);
            
            std::cerr << "Made edge: " << pb2json(nextEdge) << std::endl;
        }
    }
    
    // Spit out the graph.
    return graph;

}

void help_main(char** argv) {
    std::cerr << "usage: " << argv[0] << " [options] VGFILE VGFILE" << std::endl
        << "Compute the core graph from two graphs" << std::endl
        << std::endl
        << "options:" << std::endl
        << "    -h, --help           print this help message" << std::endl;
}

int main(int argc, char** argv) {
    
    if(argc == 1) {
        // Print the help
        help_main(argv);
        return 1;
    }
    
    optind = 1; // Start at first real argument
    bool optionsRemaining = true;
    while(optionsRemaining) {
        static struct option longOptions[] = {
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };

        int optionIndex = 0;

        switch(getopt_long(argc, argv, "h", longOptions, &optionIndex)) {
        // Option value is in global optarg
        case -1:
            optionsRemaining = false;
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
    
    // Pull out the VG file names
    std::string vgFile1 = argv[optind++];
    std::string vgFile2 = argv[optind++];
    
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
    coregraph::EmbeddedGraph embedding1(vg1, threadSet, threadSequences, getId);
    coregraph::EmbeddedGraph embedding2(vg2, threadSet, threadSequences, getId);
    
    // Trace the paths and merge the embedded graphs.
    embedding1.pinchWith(embedding2);
    
    // Make another vg graph from the thread set
    vg::VG core = pinchToVG(threadSet, threadSequences);
    
    // Spit it out to standard output
    core.serialize_to_ostream(std::cout);
    
    // Tear everything down. TODO: can we somehow run this destruction function
    // after all our other, potentially depending locals are destructed?
    stPinchThreadSet_destruct(threadSet);
    
    return 0;
}


