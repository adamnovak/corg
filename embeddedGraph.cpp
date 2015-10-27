#include "embeddedGraph.hpp"

namespace coregraph {

EmbeddedGraph::EmbeddedGraph(const xg::XG& graph, stPinchThreadSet* threadSet): threadSet(threadSet) {
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
    
    for(size_t rank = 1; rank < graph.max_node_rank(); rank++) {
        
        int64_t nodeId = graph.rank_to_id(rank);
        
        // Pull out the node's sequence, just for the length
        std::string sequence = graph.node_sequence(nodeId);
        
        std::cout << "Node: " << nodeId << ": " << sequence << std::endl;
        
        // Add a thread
        // TODO: find a free thread name, because graph IDs may overlap.
        stPinchThread* thread = stPinchThreadSet_addThread(threadSet, nodeId, 0, sequence.size());
        
        // TODO: for now just give every node its own thread.
        embedding[nodeId] = std::make_tuple(thread, 0, false);
    }
    
    // TODO: Now do the edge staples.
    

}

}
