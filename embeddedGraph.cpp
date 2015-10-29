#include "embeddedGraph.hpp"

#include <vector>
#include <set>

namespace coregraph {

EmbeddedGraph::EmbeddedGraph(const vg::VG& graph, stPinchThreadSet* threadSet,
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
        
        if(!edge.from_start()) {
            // Move the thread1 set to the end
            offset1 += (stPinchThread_getLength(thread1) - 1) * (isReverse1 ? -1 : 1);
            isReverse1 = !isReverse1;
        }
        
        if(edge.to_end()) {
            // Move the thread2 set to the end
            offset2 += (stPinchThread_getLength(thread2) - 1) * (isReverse2 ? -1 : 1);
            isReverse2 = !isReverse2;
        }
        
        // Do the welding
        stPinchThread_pinch(thread, thread1, 0, offset1, 1, isReverse1);
        stPinchThread_pinch(thread, thread2, 1, offset2, 1, isReverse2);
    });
}

void EmbeddedGraph::pinchWith(const EmbeddedGraph& other) {
    // Look for common path names
    std::set<std::string> ourPaths;
    
    graph.paths.for_each([&](vg::Path& path) {
        ourPaths.insert(path.name());
    }
    std::set<std::string> sharedPaths;
    other.graph.paths.for_each([&](vg::Path& path) {
        std::string pathName = path.name();
        if(ourPaths.count(pathName)) {
            sharedPaths.insert(pathName);
        }
    }
    
    for(std::string pathName : sharedPaths) {
        // We zip along every shared path
    
        // Get the mappings
        std::list<Mapping>& ourPath = graph.paths.get_path(pathName);
        std::list<Mapping>& theirPath = other.graph.paths.get_path(pathName);
    
        std::list<Mapping>::iterator ourMapping = ourPath.begin();
        std::list<Mapping>::iterator theirMapping = ourPath.begin();
    
        while(ourMapping != ourPath.end() && theirMapping != theirPath.end()) {
            // Go along the two paths.
            
            // Make sure the mappings are perfect matches
            
            // See how long our mapping is and how long their mapping is
            
            // See how much they overlap (start and length in each mapping)
            
            // Figure out where that overlapped region is in each graph (start, length, and orientation in each mapping's node)
            
            // Convert those to start length, and orientation in each mapping's thread
            
            // Pinch the threads
            
            // Advance the mapping that ends first, or, if both end at the same place, advance both
            
            // If you hit the end of one path before the end of the other, complain 
        
        }
        
        // We should reach the end at the same time
        
        
    }
}
    
    
}
