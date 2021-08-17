from logging import getLogger
import networkx as nx

class WitcherGraphNode:
    def __init__(self, node_id, node_type, node_address, node_size):
        self.id = node_id
        self.type = node_type
        assert(self.type == "Store" or self.type == "Load")
        self.address = node_address
        self.size = node_size

    def __repr__(self):
        return "<WitcherGraphNode: id:%d, type:%s, addr:%x, size:%d>" % \
               (self.id, self.type, self.address, self.size)

    def __str__(self):
        return "<WitcherGraphNode: id:%d, type:%s, addr:%x, size:%d>" % \
               (self.id, self.type, self.address, self.size)

class WitcherGraphEdge:
    def __init__(self, edge_type, src_id, tgt_id):
        self.type = edge_type
        assert(self.type == "data" or self.type == "ctrl")
        self.src_id = src_id
        self.tgt_id = tgt_id

    def __repr__(self):
        return "<WitcherGraphEdge: type:%s, src_id:%x, tgt_id:%d>" % \
               (self.type, self.src_id, self.tgt_id)

    def __str__(self):
        return "<WitcherGraphEdge: type:%s, src_id:%x, tgt_id:%d>" % \
               (self.type, self.src_id, self.tgt_id)

class WitcherGraph:
    def __init__(self, nodes, edges):
        self.graph = nx.DiGraph()

        self.init_nodes(nodes)
        self.init_edges(edges)
        getLogger().debug("graph nodes size: " + str(len(self.graph.nodes)))
        getLogger().debug("graph nodes: " + str(list(self.graph.nodes)))
        getLogger().debug("graph edges size: " + str(len(self.graph.edges)))
        getLogger().debug("graph edges: " + str(list(self.graph.edges)))

    def init_nodes(self, nodes):
        for node in nodes:
            self.graph.add_node(node.id)
            self.graph.nodes[node.id]["id"] = node.id
            self.graph.nodes[node.id]["type"] = node.type
            self.graph.nodes[node.id]["address"] = node.address
            self.graph.nodes[node.id]["size"] = node.size
            getLogger().debug(self.graph.nodes.data())

    def init_edges(self, edges):
        for edge in edges:
            # TODO not sure about the direction here
            self.graph.add_edge(edge.src_id, edge.tgt_id)
            self.graph.edges[edge.src_id, edge.tgt_id]["type"] = edge.type
            getLogger().debug(self.graph[edge.src_id][edge.tgt_id])

class WitcherPPDG:
    def __init__(self, ppdg_str):
        ppdg_str_list = ppdg_str.split("\n")
        ppdg_str_list = list(filter(None, ppdg_str_list))
        ppdg_str_list = ppdg_str_list[1:]

        def generate_node(node_str):
            node_id = int(node_str[0:node_str.find("[")])
            entry = node_str[node_str.find("TraceEntry")+len("TraceEntry")+1:-3]
            entry = entry.split(",")
            return WitcherGraphNode(node_id, \
                                    entry[0], \
                                    int(entry[1], 16), \
                                    int(entry[2]))

        nodes = list(filter(lambda item: not "->" in item, ppdg_str_list))
        nodes = [generate_node(node) for node in nodes]
        getLogger().debug("ppdg nodes size: " + str(len(nodes)))
        getLogger().debug("ppdg nodes: " + str(nodes))

        def generate_edge(edge_str):
            src_id = int(edge_str[0:edge_str.find("-")])
            tgt_id = int(edge_str[edge_str.find(">")+1:edge_str.find(" ")])
            edge_type = edge_str[edge_str.find("\"")+1:-3]
            return WitcherGraphEdge(edge_type, src_id, tgt_id)

        edges = list(filter(lambda item: "->" in item, ppdg_str_list))
        edges = [generate_edge(edge) for edge in edges]
        getLogger().debug("ppdg edges size: " + str(len(edges)))
        getLogger().debug("ppdg edges: " + str(edges))

        self.witcher_graph = WitcherGraph(nodes, edges)

class WitcherPPDGs:
    def __init__(self, file_name):
        getLogger().debug("ppdg file: " + file_name)
        self.file_name = file_name
        self.witcher_ppdg_list = self.init_ppdgs()

    def init_ppdgs(self):
        ppdgs = open(self.file_name).read().split("}")
        ppdgs = ppdgs[:-1]
        return [WitcherPPDG(ppdg) for ppdg in ppdgs]
