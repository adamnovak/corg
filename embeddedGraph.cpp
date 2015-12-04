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

/**
 * Return the (from) length of a Mapping, even if thgat Mapping has no edits
 * (and is implicitly a full-length perfect match). Requires the graph that the
 * Mapping is to.
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
        std::cerr << "WARNING: No shared paths exist to merge on! No bases will be merged!" << std::endl;
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
        
    
        // Make iterators to go through them together
        std::list<vg::Mapping>::iterator ourMapping = ourPath.begin();
        std::list<vg::Mapping>::iterator theirMapping = theirPath.begin();
        
        // Keep track of where we are alogn each path, so we can get mapping
        // overlap
        int64_t ourPathBase = 0;
        int64_t theirPathBase = 0;

        std::cerr << "Processing path " << pathName << std::endl;
        
        while(ourMapping != ourPath.end() && theirMapping != theirPath.end()) {
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
        
        auto ourEnd = ourPath.end();
        auto theirEnd = theirPath.end();
        
        if((ourMapping == ourPath.end()) != (theirMapping == theirPath.end())) {
            std::cerr << "We ran out of path in one graph and not in the other!" << std::endl;
            
            if(ourMapping != ourPath.end()) {
                std::cerr << "We have a mapping" << std::endl;
                std::cerr << "Our mapping: " << pb2json(*ourMapping) << std::endl;
            }
            
            if(theirMapping != theirPath.end()) {
                std::cerr << "They have a mapping" << std::endl;
                std::cerr << "Their mapping: " << pb2json(*theirMapping) << std::endl;
            }
            
            // We should reach the end at the same time, but we didn't
            throw std::runtime_error("Ran out of mappings on one path before the other!");
        }
        
    }
}
    
    
}
