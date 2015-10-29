#include "embeddedGraph.hpp"

#include <vector>
#include <set>

namespace coregraph {

EmbeddedGraph::EmbeddedGraph(const xg::XG& graph, stPinchThreadSet* threadSet,
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
    
    for(size_t rank = 1; rank <= graph.max_node_rank(); rank++) {
        
        int64_t nodeId = graph.rank_to_id(rank);
        
        // Pull out the node's sequence, just for the length
        std::string sequence = graph.node_sequence(nodeId);
        
        std::cout << "Node: " << nodeId << ": " << sequence << std::endl;
        
        // Add a thread
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, getId(), 0, sequence.size());
        
        // TODO: for now just give every node its own thread.
        embedding[nodeId] = std::make_tuple(thread, 0, false);
    }
    
    for(size_t rank = 1; rank <= graph.max_node_rank(); rank++) {
        // Now look at the edges on every node        
        int64_t nodeId = graph.rank_to_id(rank);
        
        std::vector<vg::Edge> edges = graph.edges_of(nodeId);
        
        for(auto edge : edges) {
            if(nodeId == std::min(edge.from(), edge.to())) {
                // Only look at edges from the lowest-ID node.
                
                // Make a 2-base staple sequence
                stPinchThread* thread = stPinchThreadSet_addThread(threadSet, getId(), 0, 2);
                
                // Unpack the tuples describing the embeddings
                stPinchThread* thread1, *thread2;
                int64_t offset1, offset2;
                bool isReverse1, isReverse2;
                
                std::tie(thread1, offset1, isReverse1) = embedding.at(edge.from());
                std::tie(thread2, offset2, isReverse2) = embedding.at(edge.to());
                
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
            }
        }
    }
}

void EmbeddedGraph::pinchWith(const EmbeddedGraph& other) {
    // Look for common path names
    std::set<std::string> ourPaths;
    for(size_t rank = 1; rank <= graph.max_path_rank(); rank++) {
        ourPaths.insert(graph.path_name(rank));
    }
    std::set<std::string> sharedPaths;
    for(size_t rank = 1; rank <= other.graph.max_path_rank(); rank++) {
        std::string pathName = other.graph.path_name(rank);
        if(ourPaths.count(pathName)) {
            sharedPaths.insert(pathName);
        }
    }
    
    for(std::string pathName : sharedPaths) {
        // We zip along every shared path
    
        // Now we go through the two paths side by side. We advance in lock-step
        // along the path, base by base. If either graph changes to a new node, find
        // the range over which the new node and the (new or existing) node in the
        // other graph overlap, and do a pinch.
        
        // How long is this path?
        size_t pathLength = graph.path_length(pathName);
        
        if(other.graph.path_length(pathName) != pathLength) {
            // Complain if it isn't consistent
            throw std::runtime_error("Path lengths do not match for path: " + pathName);
        }
        
        // What node ID are we on currently in each graph?
        int64_t ourNode = 0;
        int64_t theirNode = 0;
        
        // What nodes were we on at the previous path position?
        int64_t ourLastNodeId = 0;
        int64_t theirLastNodeId = 0;
        
        for(size_t i = 0; i < pathLength; i++) {
            // Look up what node is here
            ourNode = graph.node_at_path_position(pathName, i);
            theirNode = other.graph.node_at_path_position(pathName, i);
            
            vg::Path path = graph.path(pathName);
        }
        
    }
}
    
    
}
