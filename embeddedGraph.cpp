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
        if(!graph.entity_is_node(rank)) {
            // Skip anything that's not a node.
            // TODO: what are these things?
            continue;
        }
        
        size_t nodeRank = graph.entity_rank_as_node_rank(rank);
        
        int64_t nodeId = graph.rank_to_id(nodeRank);
        
        std::cout << "Node: " << nodeId << ": " << graph.node_sequence(nodeId) << std::endl;
    }

}

}
