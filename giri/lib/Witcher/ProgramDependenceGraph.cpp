#include <boost/graph/graphviz.hpp>

#include "Witcher/ProgramDependenceGraph.h"

using namespace witcher;

/*----------------------------------PDG Begins--------------------------------*/
PDG::PDG() {
}

vertex_t PDG::getVertexOrCreate(DynValue* val) {
  unordered_map<DynValue, vertex_t>::iterator it = valToVtxMap.find(*val);
  if (it != valToVtxMap.end()) {
    // if we have, simply return the vertex
    return it->second;
  } else {
    // add a new vertex
    vertex_t v = add_vertex(graph);
    // set the pointer
    graph[v] = val;
    // cache the vertex in the map
    valToVtxMap.insert(pair<DynValue, vertex_t>(*val, v));
    return v;
  }
}

edge_t PDG::addDepEdge(DynValue* source, DynValue* target) {
  // get the vertexes
  vertex_t src_vtx = getVertexOrCreate(source);
  vertex_t target_vtx = getVertexOrCreate(target);

  return GraphWrapper::addDepEdge(src_vtx, target_vtx);
}

void PDG::addDataDepEdge(DynValue* source, DynValue* target) {
  edge_t edge = addDepEdge(source, target);
  // make it as a data dependence edge
  graph[edge] = Edge::DataDep;
}

void PDG::addCtrlDepEdge(DynValue* source, DynValue* target) {
  edge_t edge = addDepEdge(source, target);
  // make it as a control dependence edge
  graph[edge] = Edge::CtrlDep;
}

void PDG::write_edge(raw_ostream& out, const edge_t& e) {
  out << "[label=\"" << EdgeType[(int) graph[e]]<< "\"]";
}

void PDG::write_vertex(raw_ostream& out, const vertex_t& v) {
  out << "[label=\"";
  graph[v]->getValue()->print(out);
  out << ":::Index:" << graph[v]->getIndex();
  out << "\"]";
}

// This is a modified version of boost::write_graphviz, which doesn't support
// raw_osream (required by llvm::Value)
void PDG::write_graphviz(raw_ostream& out) {
  // Type definitions for convenience
  using namespace boost;
  typedef typename graph_traits< Graph >::directed_category cat_type;
  typedef graphviz_io_traits<cat_type> Traits;
  typename property_map<Graph, vertex_index_t>::type
                                      vertex_id = get(vertex_index, graph);

  // dot header
  out << "digraph G {\n";

  // Vertexes
  typename graph_traits<Graph>::vertex_iterator i, end;
  for (tie(i, end) = boost::vertices(graph); i != end; ++i)
  {
      out << escape_dot_string(get(vertex_id, *i));
      write_vertex(out, *i); // print vertex attributes
      out << ";\n";
  }

  // Edges
  typename graph_traits<Graph>::edge_iterator ei, edge_end;
  for (tie(ei, edge_end) = edges(graph); ei != edge_end; ++ei)
  {
      out << escape_dot_string(get(vertex_id, boost::source(*ei, graph)))
          << Traits::delimiter()
          << escape_dot_string(get(vertex_id, boost::target(*ei, graph))) << " ";
      write_edge(out, *ei); // print edge attributes
      out << ";\n";
  }

  // dot tail
  out << "}\n";
}

bool PDG::contains(DynValue* val) {
  unordered_map<DynValue, vertex_t>::iterator it = valToVtxMap.find(*val);
  if (it != valToVtxMap.end()) {
    return true;;
  } else {
    return false;
  }
}

/*-----------------------------------PDG Ends---------------------------------*/

/*---------------------------------PPDG Begins--------------------------------*/
PPDG::PPDG() {
}

vertex_t PPDG::createVertex(DynValue* val, TraceInfo trace_info) {
  // create a vertex and add trace_info
  vertex_t v = add_vertex(graph);
  val->putTraceInfoByVertex(v, trace_info);
  graph[v] = val;

  unordered_map<DynValue, set<vertex_t>>::iterator it = valToVtxMap.find(*val);
  if (it != valToVtxMap.end()) {
    // if we already have it, then we merge those two
    // (load and store from the same memcpy)
    set<vertex_t> v_set = it->second;
    assert(v_set.size() == 1);
    vertex_t v_inplace = *v_set.begin();

    DynValue val_inplace = it->first;
    TraceInfo trace_info_inplace = val_inplace.getTraceInfoByVertex(v_inplace);
    val->putTraceInfoByVertex(v_inplace, trace_info_inplace);

    v_set.insert(v);

    valToVtxMap.erase(it);
    valToVtxMap.insert(pair<DynValue, set<vertex_t>>(*val, v_set));

    graph[v_inplace] = val;
  } else {
    // otherwise we just create one
    set<vertex_t> v_set = set<vertex_t>();
    v_set.insert(v);

    // cache the vertex in the map
    valToVtxMap.insert(pair<DynValue, set<vertex_t>>(*val, v_set));
  }

  return v;
}

set<vertex_t> PPDG::getVertices(DynValue* val) {
  unordered_map<DynValue, set<vertex_t>>::iterator it = valToVtxMap.find(*val);
  return it->second;
}

bool PPDG::contains(DynValue* val) {
  unordered_map<DynValue, set<vertex_t>>::iterator it = valToVtxMap.find(*val);
  if (it != valToVtxMap.end()) {
    return true;;
  } else {
    return false;
  }
}

void PPDG::write_edge(raw_ostream& out, const edge_t& e) {
  out << "[label=\"" << EdgeType[(int) graph[e]]<< "\"]";
}

void PPDG::write_vertex(raw_ostream& out, const vertex_t& v) {
  TraceInfo trace_info = graph[v]->getTraceInfoByVertex(v);
  out << "[label=\"";
  out << "TI:" << trace_info.getTraceIndex();
  out << ":::Src:" << trace_info.getSrcInfo();
  Entry entry = trace_info.getTraceEntry();
  out << ":::TraceEntry:";
  if (entry.type == RecordType::LDType) {
    out << "Load,";
  } else if (entry.type == RecordType::STType) {
    out << "Store,";
  } else {
    // Never come to here!
    assert(false);
  }
  out.write_hex(entry.address);
  out << "," << entry.length;
  out << "\"]";
}

// This is a modified version of boost::write_graphviz, which doesn't support
// raw_osream (required by llvm::Value)
void PPDG::write_graphviz(raw_ostream& out) {
  // Type definitions for convenience
  using namespace boost;
  typedef typename graph_traits< Graph >::directed_category cat_type;
  typedef graphviz_io_traits<cat_type> Traits;
  typename property_map<Graph, vertex_index_t>::type
                                      vertex_id = get(vertex_index, graph);

  // dot header
  out << "digraph G {\n";

  // Vertexes
  typename graph_traits<Graph>::vertex_iterator i, end;
  for (tie(i, end) = boost::vertices(graph); i != end; ++i)
  {
      out << escape_dot_string(get(vertex_id, *i));
      write_vertex(out, *i); // print vertex attributes
      out << ";\n";
  }

  // Edges
  typename graph_traits<Graph>::edge_iterator ei, edge_end;
  for (tie(ei, edge_end) = edges(graph); ei != edge_end; ++ei)
  {
      out << escape_dot_string(get(vertex_id, boost::source(*ei, graph)))
          << Traits::delimiter()
          << escape_dot_string(get(vertex_id, boost::target(*ei, graph))) << " ";
      write_edge(out, *ei); // print edge attributes
      out << ";\n";
  }

  // dot tail
  out << "}\n";
}
/*----------------------------------PPDG Ends---------------------------------*/
