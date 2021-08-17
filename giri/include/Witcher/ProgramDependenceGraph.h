#ifndef WITCHERGRAPH_H
#define WITCHERGRAPH_H

#include <unordered_map>
#include <boost/graph/adjacency_list.hpp>

#include "Giri/TraceFile.h"

using namespace giri;
using namespace std;

namespace witcher {

enum class Edge: unsigned {
  DataDep = 0, // Data Dependence Edge
  CtrlDep     // Control Dependence Edge
};

// For printing Edge Type
static string EdgeType[] = {
  "data",
  "ctrl"
};

// Abstraction from boost stuffs
typedef DynValue* Vertex;
typedef boost::adjacency_list<boost::vecS, boost::vecS,
                              boost::bidirectionalS,
                              Vertex, Edge> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor vertex_t;
typedef boost::graph_traits<Graph>::edge_descriptor edge_t;
typedef boost::graph_traits<Graph>::vertex_iterator vertex_iter;
typedef boost::graph_traits<Graph>::out_edge_iterator out_edge_iterator;


// a wrapper for boost gragh, base class for pdg and ppdg
class GraphWrapper{
protected:
  // Boost adjacency_list graph
  Graph graph;

public:
  GraphWrapper() {
  }

  // Add data dependency Edge
  void addDataDepEdge(vertex_t src_vtx, vertex_t target_vtx) {
    edge_t edge = addDepEdge(src_vtx, target_vtx);
    // make it as a data dependence edge
    graph[edge] = Edge::DataDep;
  }

  // Add control dependency Edge
  void addCtrlDepEdge(vertex_t src_vtx, vertex_t target_vtx) {
    edge_t edge = addDepEdge(src_vtx, target_vtx);
    // make it as a control dependence edge
    graph[edge] = Edge::CtrlDep;
  }

  // return the vertex iter
  std::pair<vertex_iter, vertex_iter> vertices() {
    return boost::vertices(graph);
  }

  // get the value from the vertex
  DynValue* getVertexValue(vertex_t v) {
    return graph[v];
  }

  // return the out edge iter
  std::pair<out_edge_iterator, out_edge_iterator> out_edges(vertex_t v) {
    return boost::out_edges(v, graph);
  }

  // return the target vertex of an edge
  vertex_t target(edge_t e) {
    return boost::target(e, graph);
  }

  // return true if this is a data dependence edge
  bool isDataDepEdge(edge_t e) {
    return graph[e] == Edge::DataDep;
  }

  // return true if this is a control dependence edge
  bool isCtrlDepEdge(edge_t e) {
    return graph[e] == Edge::CtrlDep;
  }

protected:
  // Helper function add the edge, without differentiate the edge type
  edge_t addDepEdge(vertex_t src_vtx, vertex_t target_vtx) {
    // add the edge in the graph
    edge_t edge;
    bool succ;
    tie(edge, succ) = boost::add_edge(src_vtx, target_vtx, graph);
    assert(succ && "Fail to insert an edge!\n");

    return edge;
  }
};

// PDG
class PDG : public GraphWrapper{
private:
  // A hash map mapping DynValues to Vertexes in the Graph
  unordered_map<DynValue, vertex_t> valToVtxMap;

public:
  PDG();
  // Check whether we have a Vertex for this value.
  // If we have, then return it; otherwise create a new one and cache it in the
  // map
  vertex_t getVertexOrCreate(DynValue* val);
  // Add data dependency Edge
  void addDataDepEdge(DynValue* source, DynValue* target);
  // Add control dependency Edge
  void addCtrlDepEdge(DynValue* source, DynValue* target);
  // Print the graph using graphviz format
  void write_graphviz(raw_ostream& out);
  // Check the whether the val is in this graph or not
  bool contains(DynValue* val);

private:
  // Helper function add the edge, without differentiate the edge type
  edge_t addDepEdge(DynValue* source, DynValue* target);
  // Writing properties of a vertex to out
  void write_vertex(raw_ostream& out, const vertex_t& v);
  // Writing properties of an edge to out
  void write_edge(raw_ostream& out, const edge_t& v);
};

// PDG
class PPDG : public GraphWrapper{
private:
  // A hash map mapping DynValue to set<Vertex> in the Graph
  // We use set<Vertex> here, because a load and a store from a same memcpy
  // share one DynValue
  unordered_map<DynValue, set<vertex_t>> valToVtxMap;

public:
  PPDG();
  // get set<vertex> from a DynValue
  set<vertex_t> getVertices(DynValue* val);
  // Create a vertex
  vertex_t createVertex(DynValue* val, TraceInfo traceInfo);
  // Check the whether the val is in this graph or not
  bool contains(DynValue* val);
  // Print the graph using graphviz format
  void write_graphviz(raw_ostream& out);

private:
  // Writing properties of a vertex to out
  void write_vertex(raw_ostream& out, const vertex_t& v);
  // Writing properties of an edge to out
  void write_edge(raw_ostream& out, const edge_t& v);
};

}
#endif
