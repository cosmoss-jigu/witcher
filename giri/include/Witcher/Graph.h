#ifndef WITCHERGRAPH_H
#define WITCHERGRAPH_H

#include "Giri/TraceFile.h"

#include <set>
#include <map>
#include <queue>

using namespace std;
using namespace giri;

namespace witcher {

// A Node used by the Graph
class Node {
public:
  Node(DynValue *dyn_value) : val(dyn_value) { }

public:
  DynValue* getVal(void) const { return val; }
  set<Node*>* getCtrlDepTo(void) { return &ctrl_dep_to; }
  set<Node*>* getCtrlDepFrom(void) { return &ctrl_dep_from; }
  set<Node*>* getDataDepTo(void) { return &data_dep_to; }
  set<Node*>* getDataDepFrom(void) { return &data_dep_from; }

private:
  // DynValue from TraceFile
  DynValue* val;
  // control dependent edges: this->nodes in ctrl_dep_to
  set<Node*> ctrl_dep_to;
  // control dependent edges: nodes in ctrl_dep_from -> this
  set<Node*> ctrl_dep_from;
  // data dependent edges: this->nodes in data_dep_to
  set<Node*> data_dep_to;
  // data dependent edges: nodes in data_dep_from -> this
  set<Node*> data_dep_from;
};

// A basic graph for PDG
class Graph {
public:
  Graph() { }

  // Check whether the graph already contains this value
  bool contains(DynValue* val) {
    return nodes.find(*val) != nodes.end();
  }

  // Add a data dependent edge: from -> to
  void addDataDepEdge(DynValue* from, DynValue* to) {
    // Check or initialize the nodes
    addToNodes(from);
    addToNodes(to);

    // Get the nodes
    Node* node_from = nodes.find(*from)->second;
    Node* node_to = nodes.find(*to)->second;

    // Add edges
    node_from->getDataDepTo()->insert(node_to);
    node_to->getDataDepFrom()->insert(node_from);

    // Update heads and tails
    // TODO could be optimized
    updateHeadOrTail(node_from);
    updateHeadOrTail(node_to);
  }

  // Add a control dependent edge: from -> to
  void addCtrlDepEdge(DynValue* from, DynValue* to) {
    // Check or initialize the nodes
    addToNodes(from);
    addToNodes(to);

    // Get the nodes
    Node* node_from = nodes.find(*from)->second;
    Node* node_to = nodes.find(*to)->second;

    // Add edges
    node_from->getCtrlDepTo()->insert(node_to);
    node_to->getCtrlDepFrom()->insert(node_from);

    // Update heads and tails
    // TODO could be optimized
    updateHeadOrTail(node_from);
    updateHeadOrTail(node_to);
  }

  // TODO dependence loop detection
  // TODO print the graph into .dot

private:
  // If the node is not inside the graph, we add it in.
  void addToNodes(DynValue* val) {
    if (!contains(val)) {
      nodes.insert(pair<DynValue, Node*>(*val, new Node(val)));
    }
  }

  // Update heads and tials
  void updateHeadOrTail(Node* node) {
    // if it is not in the heads and there are no edges coming in,
    // then we add it into the heads
    if (heads.find(node) == heads.end() &&
        node->getDataDepFrom()->empty() &&
        node->getCtrlDepFrom()->empty()) {
        heads.insert(node);
    }

    // if it is in the heads and there are edges coming in,
    // then we remove it from the heads
    if (heads.find(node) != heads.end() &&
        (!node->getDataDepFrom()->empty() ||
         !node->getCtrlDepFrom()->empty())) {
        heads.erase(node);
    }

    // if it is not in the tails and there are no edges going out,
    // then we add it into the tails
    if (tails.find(node) != tails.end() &&
        node->getDataDepTo()->empty() &&
        node->getCtrlDepTo()->empty()) {
        tails.insert(node);
    }

    // if it is in the tails and there are edges going out,
    // then we remove it from the tails
    if (tails.find(node) == tails.end() &&
        (!node->getDataDepTo()->empty() ||
         !node->getCtrlDepTo()->empty())) {
        tails.erase(node);
    }
  }

private:
  map<DynValue, Node*> nodes;
  set<Node*> heads;
  set<Node*> tails;
};

}

#endif
