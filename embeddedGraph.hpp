#ifndef COREGRAPH_EMBEDDEDGRAPH_HPP
#define COREGRAPH_EMBEDDEDGRAPH_HPP

#include <iostream>
#include <map>
#include <utility>

#include "ekg/vg/vg.hpp"

// Hack around stupid name mangling issues
extern "C" {
    #include "benedictpaten/pinchesAndCacti/inc/stPinchGraphs.h"
}

namespace coregraph {

/**
 * Represents a vg graph that has been embedded in a pinch graph, as a series
 * of pinched-together threads.
 */
class EmbeddedGraph {
public:
    /**
     * Construct an embedding of the given graph in the given thread set. Needs
     * a function that can produce unique novel sequence names.
     */
    EmbeddedGraph(const vg::VG& graph, stPinchThreadSet* threadSet, std::function<int64_t(void)> getId);
    
    /**
     * Trace out common paths between this embedded graph and the other graph
     * embedded in the same stPinchThreadSet and pinch together.
     */
    void pinchWith(const EmbeddedGraph& other);
    
    /**
     * Convert a pinch thread set to a VG graph, broken up into several protobuf
     * Graph objects, of suitable size for serialization. Graph objects are
     * streamed out through the callback.
     */
    static void threadSetToGraphs(stPinchThreadSet* threadSet, std::function<void(vg::Graph)> callback);
    
protected:
    // The graph we came from (which keeps track of the path data)
    const vg::VG& graph;

    // The thread set that the graph is embedded in.
    stPinchThreadSet* threadSet;
    
    // The embedding, mapping from xg node ID to thread, start base,
    // is reverse
    std::map<int64_t, std::tuple<stPinchThread*, int64_t, bool>> embedding;


};


}

#endif
