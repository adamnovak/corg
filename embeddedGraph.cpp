#include "embeddedGraph.hpp"

#include <vector>
#include <set>

namespace coregraph {

EmbeddedGraph::EmbeddedGraph(vg::VG& graph, stPinchThreadSet* threadSet,
    std::map<int64_t, std::string>& threadSequences,
    std::function<int64_t(void)> getId, const std::string& name): graph(graph),
    threadSet(threadSet), name(name) {
    
    // We need to construct some embedding of xg nodes in a pinch graph.

    // We can't combine any two vg nodes onto the same thread, if either of them
    // has multiple edges on either side.
    
    // So, the algorithm:
    // Go through each vg node we haven't already placed
    // If the node has degree 1 on both ends, look left and right and compose a run of such nodes
    // Be careful because the run may be circular
    // Embed all the nodes onto a thread.
    // After doing that for all the nodes, turn all remaining nodes into their own threads.
    // For every edge, if it's not implicit, make an "NN" staple and attach the nodes it wants together.
    
    graph.for_each_node([&](vg::Node* node) {
#ifdef debug
        std::cerr << "Node: " << node->id() << ": " << node->sequence() << std::endl;
#endif
        // Add a thread
        int64_t threadName = getId();
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, threadName, 0, node->sequence().size());
        // Copy over its sequence
        threadSequences[threadName] = node->sequence();
        
        // TODO: for now just give every node its own thread.
        embedding[node->id()] = std::make_tuple(thread, 0, false);
    
    });
    
    graph.for_each_edge([&](vg::Edge* edge) {
                
        // Attach the nodes as specified by the edges
                
        // Make a 2-base staple sequence
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, getId(), 0, 2);
        
        // Unpack the tuples describing the embeddings, so we're holding thread,
        // offset, is-end-of-the-node-and-not-start tuples representing the two
        // sides to weld together.
        stPinchThread* thread1, *thread2;
        int64_t offset1, offset2;
        bool isEnd1, isEnd2; // These are basically !from_start and to_end
        
        std::tie(thread1, offset1, isEnd1) = embedding.at(edge->from());
        std::tie(thread2, offset2, isEnd2) = embedding.at(edge->to());
        
        // Adapt these to point to the sequence ends we want to weld together.
        // They start out pointing to the low ends, which are the starts if the
        // nodes are not embedded in reverse, and the ends otherwise.
        
        if(!edge->from_start()) {
            // Move the thread1 set to the end
            offset1 += (stPinchThread_getLength(thread1) - 1) * (isEnd1 ? -1 : 1);
            isEnd1 = !isEnd1;
        }
        
        if(edge->to_end()) {
            // Move the thread2 set to the end
            offset2 += (stPinchThread_getLength(thread2) - 1) * (isEnd2 ? -1 : 1);
            isEnd2 = !isEnd2;
        }
        
        // Do the welding. We're holding the ends looking outwards from the
        // join, so we need to flip the orientation of one of them. Also, pinch
        // graphs use 0 for the relatively backward orientation, so we invert
        // here. We still report the orientations the VG way.
#ifdef debug
        std::cerr << "Welding 0 on staple to " << offset1 << " on " << stPinchThread_getName(thread1) <<
            " in orientation " << (isEnd1 ? "forward" : "reverse") << std::endl;
#endif
        stPinchThread_pinch(thread, thread1, 0, offset1, 1, isEnd1);
#ifdef debug
        std::cerr << "Welding 1 on staple to " << offset2 << " on " << stPinchThread_getName(thread2) <<
            " in orientation " << (isEnd2 ? "reverse" : "forward") << std::endl;
#endif
        stPinchThread_pinch(thread, thread2, 1, offset2, 1, !isEnd2);
    });
}

/**
 * Return true if a mapping is a perfect match, and false if it isn't.
 */
bool mappingIsPerfectMatch(const vg::Mapping& mapping) {
    for (auto edit : mapping.edit()) {
        if (edit.from_length() != edit.to_length() || !edit.sequence().empty()) {
            // This edit isn't a perfect match
            return false;
        }
    }
    
    // If we get here, all the edits are perfect matches.
    // Note that Mappings with no edits at all are full-length perfect matches.
    return true;
}

bool EmbeddedGraph::isCoveredByPaths() {
    bool covered = true;
    
    graph.for_each_node([&](vg::Node* node) {
        if(!graph.paths.has_node_mapping(node)) {
            // We found a node that doesn't have a path on it.
            covered = false;
#ifdef debug
            std::cerr << "Node: " << node->id() << ": " << node->sequence() << " is uncovered by any path" << std::endl;
#endif
            
        }
    
    });
    
    // Return the flag we've been updating
    return covered;
}

/**
 * Return the (from) length of a Mapping, even if thgat Mapping has no edits
 * (and is implicitly a full-length perfect match). Requires the graph that the
 * Mapping is to.
 * TODO: put in a util file or something.
 */
int64_t mappingLength(const vg::Mapping& mapping, vg::VG& graph) {
    if(mapping.edit_size() == 0) {
        // There are no edits so we just use the (remaining) length of the node.
        
         if(mapping.is_reverse()) {
            // We take the part left of its offset. We don't even need the node.
            // But we account for the 0-based-ness of the coordinates.
            return mapping.position().offset() + 1;
        } else {
            // We take the part at and right of the offset
            int64_t nodeLength = graph.get_node(mapping.position().node_id())->sequence().size();
            return nodeLength - mapping.position().offset();       
       }
    } else {
        // There are edits so just refer to them
        return vg::mapping_from_length(mapping);
    }
}

const std::string& EmbeddedGraph::getName() {
    return name;
}

size_t EmbeddedGraph::scanPath(std::list<vg::Mapping>& path) {
    
    size_t totalLength = 0;
    
    for(auto& mapping : path) {
        // Force it to be a perfect mapping
        assert(mappingIsPerfectMatch(mapping));
        
        // Calculate and incorporate its length
        totalLength += mappingLength(mapping, graph);
    }
    
    return totalLength;
}

void EmbeddedGraph::pinchWith(EmbeddedGraph& other) {
    // Look for common path names
    std::set<std::string> ourPaths;
    
    graph.paths.for_each([&](vg::Path& path) { 
        ourPaths.insert(path.name());
    });
    std::set<std::string> sharedPaths;
    other.graph.paths.for_each([&](vg::Path& path) { 
        std::string pathName = path.name();
        if(ourPaths.count(pathName)) {
            sharedPaths.insert(pathName);
        }
    });
    
    if(sharedPaths.size() == 0) {
        // Warn the user that no merging can happen.
        std::cerr << "WARNING: No shared paths exist to merge on!" << std::endl;
    }
    
    for(std::string pathName : sharedPaths) {
        // We zip along every shared path
    
        // Get the mappings
        std::list<vg::Mapping>& ourPath = graph.paths.get_path(pathName);
        std::list<vg::Mapping>& theirPath = other.graph.paths.get_path(pathName);
    
        // Go through each and make sure their lengths agree.
        std::cerr << "Checking " << pathName << " in " << name << " graph." << std::endl;
        size_t ourLength = scanPath(ourPath);
        std::cerr << "Checking " << pathName << " in " << other.name << " graph." << std::endl;
        size_t theirLength = other.scanPath(theirPath);
        
        if(ourLength != theirLength) {
            // These graphs disagree and we can't merge them without risking merging on an offset.
            std::cerr << "Path length mismatch for " << pathName << ": " << ourLength << 
                " in " << name << " vs. " << theirLength << " in " << other.name << std::endl;
            throw std::runtime_error("Path length mismatch");
        }
        
        std::cerr << "Processing path " << pathName << std::endl;
        
        // Do thje actual merge
        pinchOnPaths(ourPath, other, theirPath);
    }
}

void EmbeddedGraph::pinchOnPaths(std::list<vg::Mapping>& path, EmbeddedGraph& other, 
    std::list<vg::Mapping>& otherPath) {
        
    // Make iterators to go through them together
    std::list<vg::Mapping>::iterator ourMapping = path.begin();
    std::list<vg::Mapping>::iterator theirMapping = otherPath.begin();
    
    // Keep track of where we are alogn each path, so we can get mapping
    // overlap
    int64_t ourPathBase = 0;
    int64_t theirPathBase = 0;

    while(ourMapping != path.end() && theirMapping != otherPath.end()) {
        // Go along the two paths.
#ifdef debug
        std::cerr << "At " << ourPathBase << " in graph 1, " << theirPathBase << " in graph 2." << std::endl;
        
        std::cerr << "Our mapping: " << pb2json(*ourMapping) << std::endl;
        std::cerr << "Their mapping: " << pb2json(*theirMapping) << std::endl;
#endif
        
        // Make sure the mappings are perfect matches
        assert(mappingIsPerfectMatch(*ourMapping));
        assert(mappingIsPerfectMatch(*theirMapping));
        
        // See how long our mapping is and how long their mapping is
        int64_t ourMappingLength = mappingLength(*ourMapping, graph);
        int64_t theirMappingLength = mappingLength(*theirMapping, other.graph);

#ifdef debug
        std::cerr << "Our mapping is " << ourMappingLength << " bases on node " << (*ourMapping).position().node_id() <<
            " offset " << (*ourMapping).position().offset() << " orientation " << (*ourMapping).is_reverse() << std::endl;
        std::cerr << "Their mapping is " << theirMappingLength << " bases on node " << (*theirMapping).position().node_id() <<
            " offset " << (*theirMapping).position().offset() << " orientation " << (*theirMapping).is_reverse() << std::endl;
#endif

        // See how much they overlap (start and length in each mapping)
        if(ourPathBase < theirPathBase + theirMappingLength &&
            theirPathBase < ourPathBase + ourMappingLength) {
            // The two ranges do overlap
            // Find their first base
            int64_t overlapStart = std::max(ourPathBase, theirPathBase);
            // And their past-the-end base
            int64_t overlapEnd = std::min(ourPathBase + ourMappingLength,
                theirPathBase + theirMappingLength);    
            // How long soes that make the overlap?
            int64_t overlapLength = overlapEnd - overlapStart;
            
#ifdef debug
            std::cerr << "The mappings overlap for " << overlapLength << " bp" << std::endl;
#endif
            
            // Figure out where that overlapped region is in each graph
            // (start, length, and orientation in each mapping's node).
            // Start at the positions where the nodes start.
            stPinchThread* ourThread, *theirThread;
            int64_t ourOffset, theirOffset;
            bool ourIsReverse, theirIsReverse;
            
            std::tie(ourThread, ourOffset, ourIsReverse) = embedding.at((*ourMapping).position().node_id());
            std::tie(theirThread, theirOffset, theirIsReverse) = other.embedding.at((*theirMapping).position().node_id());
            
            // Advance by the offset in the node at which the mapping starts
            ourOffset += (*ourMapping).position().offset() * (ourIsReverse ? -1 : 1);
            theirOffset += (*theirMapping).position().offset() * (theirIsReverse ? -1 : 1);
            
            // Advance up to the start of the overlap, accounting for the
            // orientation of both the mapping in the node and the node in
            // the thread.
            ourOffset += (overlapStart - ourPathBase) * (ourIsReverse != (*ourMapping).is_reverse() ? -1 : 1);
            theirOffset += (overlapStart - theirPathBase) * (theirIsReverse != (*theirMapping).is_reverse() ? -1 : 1);
            
            // Pull back to the actual start of the overlap in thread
            // coordinates if it is going backward on the thread in question
            // from the mapping's start position. The -1 accounts for the
            // inclusiveness of the original end coordinate, and going to an
            // end-exclusive system.
            if(ourIsReverse != (*ourMapping).is_reverse()) {
                ourOffset -= overlapLength - 1;
            }
            if(theirIsReverse != (*theirMapping).is_reverse()) {
                theirOffset -= overlapLength - 1;
            }
            
            // Should we pinch the things relatively forward (0) or
            // relatively reverse (1)? Calculated by xor-ing all the flags
            // that could, by themselves, cause us to pinch in opposite
            // orientations.
            bool relativeOrientation = (ourIsReverse != (*ourMapping).is_reverse() != 
                theirIsReverse != (*theirMapping).is_reverse());

#ifdef debug
            std::cerr << "Pinch thread " << stPinchThread_getName(ourThread) << ":" << ourOffset << " and " << 
                stPinchThread_getName(theirThread) << ":" << theirOffset << " for " << overlapLength <<
                " bases in orientation " << (relativeOrientation ? "reverse" : "forward") << std::endl;
#endif
            
            // Pinch the threads, making sure to convert to pinch graph orientations, which are backward.
            stPinchThread_pinch(ourThread, theirThread, ourOffset, theirOffset, overlapLength, !relativeOrientation);

            
        }
        
        // Advance the mapping that ends first, or, if both end at the same place, advance both
        int64_t minNextBase = std::min(ourPathBase + ourMappingLength, theirPathBase + theirMappingLength);
        if(ourPathBase + ourMappingLength == minNextBase) {
            // We end first, so advance us
            ourPathBase = minNextBase;
            ++ourMapping;
#ifdef debug
            std::cerr << "Advanced in our thread" << std::endl;
#endif
        }
        if(theirPathBase + theirMappingLength == minNextBase) {
            // They end first, so advance them
            theirPathBase = minNextBase;
            ++theirMapping;
#ifdef debug
            std::cerr << "Advanced in their thread" << std::endl;
#endif
        }
        
        // If you hit the end of one path before the end of the other, complain 
    
    }
    
    if((ourMapping == path.end()) != (theirMapping == otherPath.end())) {
        std::cerr << "We ran out of path in one graph and not in the other!" << std::endl;
        
        if(ourMapping != path.end()) {
            std::cerr << "We have a mapping" << std::endl;
            std::cerr << "Our mapping: " << pb2json(*ourMapping) << std::endl;
        }
        
        if(theirMapping != otherPath.end()) {
            std::cerr << "They have a mapping" << std::endl;
            std::cerr << "Their mapping: " << pb2json(*theirMapping) << std::endl;
        }
        
        // We should reach the end at the same time, but we didn't
        throw std::runtime_error("Ran out of mappings on one path before the other!");
    }
        
}

std::list<vg::Mapping> EmbeddedGraph::makeMinimalPath(
    std::string& kmer, std::list<vg::NodeTraversal>::iterator occurrence,
    int offset, std::list<vg::NodeTraversal>& path) {

    // Generate a path (std::list<vg::Mapping>) that describes only the
    // kmer.
    std::list<vg::Mapping> minimalPath;
    
    // How many bases of the kmer are yet to be accounted for?
    size_t remainingKmerLength = kmer.size();
    
    // What will the offset from where we enter the next node be? Will only
    // be nonzero on the first node.
    size_t nextOffset = offset;
    
    for(std::list<vg::NodeTraversal>::iterator i = occurrence; i != path.end() && remainingKmerLength > 0; ++i) {
        // For every node the kmer visits
        
        // Make a Mapping to this node
        vg::Mapping mapping;
        
        // How long is the node we're mapping to?
        size_t nodeLength = (*i).node->sequence().size();
        
        // Set the node ID we map to
        mapping.mutable_position()->set_node_id((*i).node->id());
        // Set the offset we map to
        mapping.mutable_position()->set_offset(nextOffset);
        if((*i).backward) {
            // Adjust so we actually come to the right side instead of the
            // left.
            mapping.set_is_reverse(true);
            
            // We need to correct the offset to count from the start of the
            // underlying, reversed node, instead of the start of the
            // traversal.
            mapping.mutable_position()->set_offset(nodeLength - nextOffset - 1);
        }
        
        // Populate the length of the mapping, in a single perfect match
        // edit.
        vg::Edit* edit = mapping.add_edit();
        
        if(remainingKmerLength >= (nodeLength - nextOffset)) {
            // We don't finish with this node. Map over all of it.
            edit->set_from_length(nodeLength - nextOffset);
            edit->set_to_length(nodeLength - nextOffset);
            
            // Update the remaining kmer length to account for how much we
            // used in this node.
            remainingKmerLength -= (nodeLength - nextOffset);
        } else {
            // We do finish in this node. Map over only the part we actually
            // cover.
            edit->set_from_length(remainingKmerLength);
            edit->set_to_length(remainingKmerLength);
            
            // We consumed the rest of the kmer length.
            remainingKmerLength = 0;
        }
        
        // Adjust the offset for the next iteration. It can only be nonzero
        // for the first node.
        nextOffset = 0;
        
        // Add the mapping to the path
        minimalPath.push_back(mapping);
    }
    
    // Spit back the minimal path we have constructed, with only the Mappings
    // covering the actual kmer sequence.
    return minimalPath;

}

bool EmbeddedGraph::paths_equal(std::list<vg::Mapping>& path1, std::list<vg::Mapping>& path2) {
    // We're going to structurally compare paths that we know are made by makeMinimalPath.
    // We therefore can look at only certain fields.
    
    if(path1.size() != path2.size()) {
        // They can't be equal if they differ in number of mappings.
        return false;
    }
    
    // Loop through the two paths in parallel. See
    // <http://stackoverflow.com/a/19933798>
    auto i = path1.begin();
    auto j = path2.begin();
    
    for(; i != path1.end() && j != path2.end(); ++i, ++j) {
        auto& mapping1 = *i;
        auto& mapping2 = *j;
        
        // Compare all the fields of the mappings
        
        if(mapping1.position().node_id() != mapping2.position().node_id()) {
            return false;
        }
        
        if(mapping1.position().offset() != mapping2.position().offset()) {
            return false;
        }
        
        if(mapping1.is_reverse() != mapping2.is_reverse()) {
            return false;
        }
        
        if(mapping1.edit_size() != mapping2.edit_size()) {
            return false;
        }
        
        for(int k = 0; k < mapping1.edit_size(); k++) {
            auto& edit1 = mapping1.edit(k);
            auto& edit2 = mapping2.edit(k);
            
            // Compare all the fields of each edit
            
            if(edit1.from_length() != edit2.from_length()) {
                return false;
            }
            
            if(edit1.to_length() != edit2.to_length()) {
                return false;
            }
        }
    }
    
    // If we get here, all the mappings match
    return true;
}

std::list<vg::Mapping> EmbeddedGraph::reverse_path(std::list<vg::Mapping> path) {
    // We need a function variable so we can supply a non-const reference.
    std::function<int64_t(int64_t)> getNodeLength = [&](int64_t nodeId) {
        // We need to be able to provide the sizes of nodes to do
        // this, and we get those sizes from our graph.
        return graph.get_node(nodeId)->sequence().size();
    };
    
    std::list<vg::Mapping> pathRev;
    for(auto& mapping : path) {
        // Reverse each mapping
        vg::Mapping reversed = vg::reverse_mapping(mapping, getNodeLength);
        
        // Put the mapping at the front of our new list (to reverse the order)
        pathRev.push_front(reversed);
    }
    
    return pathRev;
}

void EmbeddedGraph::pinchOnKmers(vg::Index& ourIndex, EmbeddedGraph& other,
    vg::Index& theirIndex, size_t kmerSize, size_t edgeMax) {
    
    // Actually good strategy:
    // Loop through the kmer instances in our index
    // For each first kmer instance followed by a different kmer (or for the last kmer instance if it's the first with its value)
    // Look it up in the other index and see if it's unique there too
    // If so, get the starting point
    // Search out a path that matches the kmer
    // Do the same to get the path in the other graph
    // Merge the paths together (make tow std::list<vg::Mapping> lists and use the path merge code)
    
    // Alternate easy startegy that I will use:
    
    // Keep track of the paths for unique kmers in our graph.
    // A kmer that occurs along multiple paths, but which the index still thinks is unique, 
    std::map<std::string, std::list<vg::Mapping>> ourUniqueKmerPaths;
    // We need to protect it with a mutex
    std::mutex ourUniqueKmerPathsMutex;
    
    // And in the other graph
    std::map<std::string, std::list<vg::Mapping>> theirUniqueKmerPaths;
    // We need to protect it with a mutex
    std::mutex theirUniqueKmerPathsMutex;
    
    #ifdef debug
        std::cerr << "Looking for kmers of size " << kmerSize << "." << std::endl;
    #endif
    
    auto observeKmer = [this](std::string& kmer,
        std::list<vg::NodeTraversal>::iterator occurrence, int offset,
        std::list<vg::NodeTraversal>& path, vg::Index& index,
        std::map<std::string, std::list<vg::Mapping>>& uniqueKmerPaths,
        std::mutex& uniqueKmerPathsMutex) {
        
        // We receive each kmer, starting at the given offset from the left of
        // the given traversal, along the given path.
        
        // We will make sure it is unique in our graph, and then add it to our
        // table of unique kmers.
        
        if(index.approx_size_of_kmer_matches(kmer) > MAX_UNIQUE_KMER_BYTES) {
            // If its data takes up lots of space, it's not unique
            return;
        }
        
        // Count up how many times it occurs
        size_t kmerCount = 0;
        index.for_kmer_range(kmer, [&](std::string& key, std::string& value) {
            kmerCount++;
        });
        
        // Also include occurrences of the reverse complement
        index.for_kmer_range(vg::reverse_complement(kmer), [&](std::string& key, std::string& value) {
            kmerCount++;
        });
        
#ifdef debug
        #pragma omp critical(cerr)
        std::cerr << "Kmer " << kmer << " occurs " << kmerCount << " times in " << getName() << "." << std::endl;
#endif
        
        if(kmerCount > 1) {
            // It's not unique in our graph
            return;
        }
        
        // If we get here it occurs only in one place in our graph.
        // But does it occur on multiple paths?
        
        // Get the minimal path for the kmer
        std::list<vg::Mapping> minimalPath(makeMinimalPath(kmer, occurrence, offset, path));
        
        // Compute what it would look like as a reverse complement
        std::list<vg::Mapping> minimalPathRev = reverse_path(minimalPath);
        
        // Now we need to do serial access to the deduplication index
        std::lock_guard<std::mutex> guard(uniqueKmerPathsMutex);
        
        // Look up the kmer in the index
        auto kv = uniqueKmerPaths.find(kmer);
        
        // And the reverse version
        auto reverse_kv = uniqueKmerPaths.find(vg::reverse_complement(kmer));
        
        if(kv == uniqueKmerPaths.end()) {
            if(reverse_kv == uniqueKmerPaths.end()) {
                // It's not in there and neither is its reverse complement.
                // Add it with the path we just made.
                uniqueKmerPaths[kmer] = minimalPath;
#ifdef debug
                #pragma omp critical(cerr)
                std::cerr << "Found unique kmer " << kmer << "." << std::endl;
#endif
            } else {
                // The reverse complement is in but this kmer isn't.
                // Find the path the reverse complement is using.
                auto& oldPath = (*reverse_kv).second;
                
                if(oldPath.size() == 0) {
                    // If it's in there with an empty minimal path, it's already
                    // a dupe. Do nothing.
                } else if(paths_equal(oldPath, minimalPathRev)) {
                    // If it's in there with a nonempty minimal path and it
                    // matches the one for our reverse complement, do nothing.
                } else {
                    // If it's in there with a nonempty minimal path and it
                    // doesn't match the one we just made, empty its path to
                    // mark it as a duplicate.
                    oldPath.clear();
                    
#ifdef debug
                    #pragma omp critical(cerr)
                    std::cerr << "Formerly unique kmer " << kmer << " is now RC-duplicated." << std::endl;
#endif
                    
                }
            }
        } else {
            // This kmer is in.
            // Make a reference to the path used.
            auto& oldPath = (*kv).second;
            
            if(oldPath.size() == 0) {
                // If it's in there with an empty minimal path, it's already a
                // dupe. Do nothing.
            } else if(paths_equal(oldPath, minimalPath)) {
                // If it's in there with a nonempty minimal path and it matches
                // the one we just made, do nothing.
            } else {
                // If it's in there with a nonempty minimal path and it doesn't match
                // the one we just made, empty its path to mark it as a duplicate.
                oldPath.clear();
                
                if(reverse_kv != uniqueKmerPaths.end()) {
                    // The reverse complement is also in, so we need to
                    // clear it too.
                    (*reverse_kv).second.clear();
                }
                
#ifdef debug
                    #pragma omp critical(cerr)
                    std::cerr << "Formerly unique kmer " << kmer << " is now duplicated." << std::endl;
#endif
            }
        }
        
        // The lock guard automatically unlocks
    
    };
    
    // Enumerate kmers in one graph with for_each_kmer_parallel
    graph.for_each_kmer_parallel(kmerSize, edgeMax, [&](std::string& kmer,
        std::list<vg::NodeTraversal>::iterator occurrence, int offset,
        std::list<vg::NodeTraversal>& path, vg::VG& kmer_graph) {
        
        // We receive each kmer, starting at the given offset from the left of
        // the given traversal, along the given path.
        
        // Observe the kmer for us
        observeKmer(kmer, occurrence, offset, path, ourIndex, ourUniqueKmerPaths, ourUniqueKmerPathsMutex);
        
    }, true, false); // Accept duplicate kmers, but not kmers with negative offsets.
    
    // Do the same for the other graph
    other.graph.for_each_kmer_parallel(kmerSize, edgeMax, [&](std::string& kmer,
        std::list<vg::NodeTraversal>::iterator occurrence, int offset,
        std::list<vg::NodeTraversal>& path, vg::VG& kmer_graph) {
        
        // Observe the kmer for them
        observeKmer(kmer, occurrence, offset, path, theirIndex, theirUniqueKmerPaths, theirUniqueKmerPathsMutex);
        
    }, true, false); // Accept duplicate kmers, but not kmers with negative offsets.
    
    // How many shared unique kmers do we find?
    size_t sharedUniqueKmers = 0;
    
    // Then find the paths for corresponding kmers and merge on them.
    for(auto& kv : ourUniqueKmerPaths) {
        // For each kmer, path pair
        if(kv.second.empty()) {
            // This was really duplicated
            continue;
        }
        
        // Look up the forward and reverse versions
        auto theirMatch = theirUniqueKmerPaths.find(kv.first);
        auto theirReverseMatch = theirUniqueKmerPaths.find(vg::reverse_complement(kv.first));
        
        if(theirMatch != theirUniqueKmerPaths.end()) {
            // If the other graph has it, find out where
            auto& theirPath = (*theirMatch).second;
            
            if(theirPath.empty()) {
                // This was really duplicated
                continue;
            }
            
            // Merge on the paths
            pinchOnPaths(kv.second, other, theirPath);
            
#ifdef debug
            std::cerr << "Mutually unique kmer " << kv.first << " pinched on." << std::endl;
#endif
            sharedUniqueKmers++;
            
        } else if(theirReverseMatch != theirUniqueKmerPaths.end()) {
            // If the other graph has it reverse complemented, find out where
            auto& theirReversePath = (*theirReverseMatch).second;
            
            if(theirReversePath.empty()) {
                // This was really duplicated
                continue;
            }
            
            // Flip it to be forward relative to us.
            auto theirPath = other.reverse_path(theirReversePath);
            
            // Merge on the paths
            pinchOnPaths(kv.second, other, theirPath);
            
#ifdef debug
            std::cerr << "RC-mutually unique kmer " << kv.first << " pinched on." << std::endl;
#endif
            sharedUniqueKmers++;
            
        }
        
    }
    
    // Report to the user what happened.
    std::cerr << "Pinched on " << sharedUniqueKmers << " shared unique " << kmerSize << "-mers." << std::endl;
    
    if(sharedUniqueKmers == 0) {
        std::cerr << "WARNING: no kmer pinches performed!" << std::endl;
    }
}
    
    
}











