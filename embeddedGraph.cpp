#include "embeddedGraph.hpp"

#include <vector>
#include <set>

namespace coregraph {

EmbeddedGraph::EmbeddedGraph(vg::VG& graph, stPinchThreadSet* threadSet,
    std::function<int64_t(void)> getId): graph(graph), threadSet(threadSet) {
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
    
        std::cout << "Node: " << node->id() << ": " << node->sequence() << std::endl;
        
        // Add a thread
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, getId(), 0, node->sequence().size());
        
        // TODO: for now just give every node its own thread.
        embedding[node->id()] = std::make_tuple(thread, 0, false);
    
    });
    
    graph.for_each_edge([&](vg::Edge* edge) {
                
        // Attach the nodes as specified by the edges
                
        // Make a 2-base staple sequence
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, getId(), 0, 2);
        
        // Unpack the tuples describing the embeddings
        stPinchThread* thread1, *thread2;
        int64_t offset1, offset2;
        bool isReverse1, isReverse2;
        
        std::tie(thread1, offset1, isReverse1) = embedding.at(edge->from());
        std::tie(thread2, offset2, isReverse2) = embedding.at(edge->to());
        
        // Adapt these to point to the sequence ends we want to weld together.
        // They start out pointing to the starts
        
        if(!edge->from_start()) {
            // Move the thread1 set to the end
            offset1 += (stPinchThread_getLength(thread1) - 1) * (isReverse1 ? -1 : 1);
            isReverse1 = !isReverse1;
        }
        
        if(edge->to_end()) {
            // Move the thread2 set to the end
            offset2 += (stPinchThread_getLength(thread2) - 1) * (isReverse2 ? -1 : 1);
            isReverse2 = !isReverse2;
        }
        
        // Do the welding
        stPinchThread_pinch(thread, thread1, 0, offset1, 1, isReverse1);
        stPinchThread_pinch(thread, thread2, 1, offset2, 1, isReverse2);
    });
}

/**
 * Return true if a mapping is a perfect match, and false if it isn't.
 */
bool mapping_is_perfect_match(const vg::Mapping& mapping) {
    for (auto edit : mapping.edit()) {
        if (edit.from_length() != edit.to_length() || !edit.sequence().empty()) {
            // This edit isn't a perfect match
            return false;
        }
    }
    
    // If we get here, all the edits are perfect matches
    return true;
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
    
    for(std::string pathName : sharedPaths) {
        // We zip along every shared path
    
        // Get the mappings
        std::list<vg::Mapping>& ourPath = graph.paths.get_path(pathName);
        std::list<vg::Mapping>& theirPath = other.graph.paths.get_path(pathName);
    
        std::list<vg::Mapping>::iterator ourMapping = ourPath.begin();
        std::list<vg::Mapping>::iterator theirMapping = ourPath.begin();
        
        // Keep track of where we are alogn each path, so we can get mapping
        // overlap
        int64_t ourPathBase = 0;
        int64_t theirPathBase = 0;

        std::cerr << "Processing path: " << pathName << std::endl;
    
        while(ourMapping != ourPath.end() && theirMapping != theirPath.end()) {
            // Go along the two paths.
            
            std::cerr << "At " << ourPathBase << " in graph 1, " << theirPathBase << " in graph 2." << std::endl;
            
            std::cerr << "Our mapping: " << pb2json(*ourMapping) << std::endl;
            std::cerr << "Their mapping: " << pb2json(*theirMapping) << std::endl;
            
            // Make sure the mappings are perfect matches
            assert(mapping_is_perfect_match(*ourMapping));
            assert(mapping_is_perfect_match(*theirMapping));
            
            // See how long our mapping is and how long their mapping is
            int64_t ourMappingLength = vg::mapping_from_length(*ourMapping);
            int64_t theirMappingLength = vg::mapping_from_length(*theirMapping);
            
            std::cerr << "Our mapping is " << ourMappingLength << " bases on node " << (*ourMapping).position().node_id() <<
                " offset " << (*ourMapping).position().offset() << " orientation " << (*ourMapping).is_reverse() << std::endl;
            std::cerr << "Their mapping is " << theirMappingLength << " bases on node " << (*theirMapping).position().node_id() <<
                " offset " << (*theirMapping).position().offset() << " orientation " << (*theirMapping).is_reverse() << std::endl;
            
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
                
                // Figure out where that overlapped region is in each graph
                // (start, length, and orientation in each mapping's node).
                // Start at the positions where the nodes start.
                stPinchThread* ourThread, *theirThread;
                int64_t ourOffset, theirOffset;
                bool ourIsReverse, theirIsReverse;
                
                std::tie(ourThread, ourOffset, ourIsReverse) = embedding.at((*ourMapping).position().node_id());
                std::tie(theirThread, theirOffset, theirIsReverse) = embedding.at((*theirMapping).position().node_id());
                
                // Advance by the offset in the node at which the mapping starts
                ourOffset += (*ourMapping).position().offset() * (ourIsReverse ? -1 : 1);
                theirOffset += (*theirMapping).position().offset() * (theirIsReverse ? -1 : 1);
                
                // Advance up to the start of the overlap, accounting for the
                // orientation of both the mapping in the node and the node in
                // the thread.
                ourOffset += (overlapStart - ourPathBase) * (ourIsReverse != (*ourMapping).is_reverse() ? -1 : 1);
                theirOffset += (overlapStart - theirPathBase) * (theirIsReverse != (*theirMapping).is_reverse() ? -1 : 1);
                
                // Pinch the threads, xor-ing all the flags that could, by
                // themselves, cause us to pinch in opposite orientations.
                stPinchThread_pinch(ourThread, theirThread, ourOffset, theirOffset, overlapLength,
                    ourIsReverse != (*ourMapping).is_reverse() != theirIsReverse != (*theirMapping).is_reverse());
                
            }
            
            // Advance the mapping that ends first, or, if both end at the same place, advance both
            int64_t minNextBase = std::min(ourPathBase + ourMappingLength, theirPathBase + theirMappingLength);
            if(ourPathBase + ourMappingLength == minNextBase) {
                // We end first, so advance us
                ourPathBase = minNextBase;
                ++ourMapping;
            }
            if(theirPathBase + theirMappingLength == minNextBase) {
                // They end first, so advance them
                theirPathBase = minNextBase;
                ++theirMapping;
            }
            
            // If you hit the end of one path before the end of the other, complain 
        
        }
        
        if((ourMapping == ourPath.end()) != (theirMapping == theirPath.end())) {
            // We should reach the end at the same time, but we didn't
            throw std::runtime_error("Ran out of mappings on one path before the other!");
        }
        
    }
}
    
    
}
