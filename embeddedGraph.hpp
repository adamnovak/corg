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
     * a place to deposit the sequences for the new threads it creates, and a
     * function that can produce unique novel sequence names. Optionally, a
     * string name can be given to the graph, although the passed string does
     * not need to outlive the graph (as it is copied).
     */
    EmbeddedGraph(vg::VG& graph, stPinchThreadSet* threadSet, std::map<int64_t, std::string>& threadSequences, 
        std::function<int64_t(void)> getId, const std::string& name="");
    
    /**
     * Trace out common paths between this embedded graph and the other graph
     * embedded in the same stPinchThreadSet and pinch together.
     */
    void pinchWith(EmbeddedGraph& other);
    
    /**
     * Convert a pinch thread set to a VG graph, broken up into several protobuf
     * Graph objects, of suitable size for serialization. Graph objects are
     * streamed out through the callback.
     */
    static void threadSetToGraphs(stPinchThreadSet* threadSet, std::function<void(vg::Graph)> callback);
    
protected:

    /**
     * Scan along a path, and ensure that it is all perfect mappings. Returns
     * the total length. The passed path must be part of this graph, not another
     * graph.
     */
    size_t scanPath(std::list<vg::Mapping>& path);

    // The graph we came from (which keeps track of the path data)
    vg::VG& graph;

    // The thread set that the graph is embedded in.
    stPinchThreadSet* threadSet;
    
    // The embedding, mapping from xg node ID to thread, start base,
    // is reverse
    std::map<int64_t, std::tuple<stPinchThread*, int64_t, bool>> embedding;
    
    // This is the name we carry around. We keep our own copy.
    std::string name;


};


}

#endif
