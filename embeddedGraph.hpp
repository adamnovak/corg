#ifndef COREGRAPH_EMBEDDEDGRAPH_HPP
#define COREGRAPH_EMBEDDEDGRAPH_HPP

#include <iostream>
#include <map>
#include <utility>

#include "ekg/xg/xg.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

namespace coregraph {

/**
 * Represents an xg graph that has been embedded in a pinch graph, as a series
 * of pinched-together threads.
 */
class EmbeddedGraph {
public:
    /**
     * Construct an embedding of the given graph in the given thread set.
     */
    EmbeddedGraph(const xg::XG& graph, stPinchThreadSet* threadSet);
    
protected:
    // The thread set that the graph is embedded in.
    stPinchThreadSet* threadSet;
    
    // The embedding, mapping from xg node ID to thread ID, start base,
    // is reverse
    std::map<int64_t, std::tuple<int64_t, int64_t, bool>> embedding;


};


}

#endif
