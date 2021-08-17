from misc.utils import Rangeable, range_cmp
from logging import getLogger
from collections import deque
import networkx as nx

class PMObject(Rangeable):
    def __init__(self, address, size):
        self.address = address
        self.size = size
        self.id = -1

    def __repr__(self):
        return "addr: " + hex(self.address) + \
               " size: " + str(self.size) + \
               " id: " + str(self.id)

    def __str__(self):
        return "addr: " + hex(self.address) + \
               " size: " + str(self.size) + \
               " id: " + str(self.id)

    # Rangeable implementation
    def get_base_address(self):
        return self.address

    # Rangeable implementation
    def get_max_address(self):
        return self.address + self.size

class PMObjectList():
    def __init__(self):
        self.list = []

    def find(self, address, size):
        assert(len(self.list) > 0)

        left = 0
        right = len(self.list) - 1
        tgt_obj = PMObject(address, size)
        while left+1 < right:
            mid = left + int((right-left) / 2)
            mid_obj = self.list[mid]
            cmp_res = range_cmp(mid_obj, tgt_obj)
            if cmp_res == 0:
                # TODO in case we miss any stores in ppdg
                #assert(mid_obj.get_base_address() <= tgt_obj.get_base_address())
                #assert(mid_obj.get_max_address() >= tgt_obj.get_max_address())
                return mid_obj
            elif cmp_res == 1:
                right = mid
            else:
                left = mid

        left_obj = self.list[left]
        if range_cmp(left_obj, tgt_obj) == 0:
            assert(left_obj.get_base_address() <= tgt_obj.get_base_address())
            assert(left_obj.get_max_address() >= tgt_obj.get_max_address())
            return left_obj

        right_obj = self.list[right]
        if range_cmp(right_obj, tgt_obj) == 0:
            assert(right_obj.get_base_address() <= tgt_obj.get_base_address())
            assert(right_obj.get_max_address() >= tgt_obj.get_max_address())
            return right_obj

        # in case we give up some ppdg
        # assert(False)
        return None

    def try_merge(self, index):
        curr = index
        # try merge the left one
        if curr-1 >= 0:
            left = curr - 1
            obj_l = self.list[left]
            obj_c = self.list[curr]
            # if overlap
            if range_cmp(obj_l, obj_c) == 0:
                assert(obj_l.get_base_address() <= obj_c.get_base_address())
                max_address = max(obj_l.get_max_address(),
                                  obj_c.get_max_address())
                # override the left one and delete the current one
                obj_l.size = max_address - obj_l.get_base_address()
                del self.list[curr]
                curr = curr - 1
        # try merge the right ones
        while curr+1 < len(self.list):
            right = curr + 1
            obj_r = self.list[right]
            obj_c = self.list[curr]
            if range_cmp(obj_c, obj_r) != 0:
                break
            assert(obj_c.get_base_address() <= obj_r.get_base_address())
            # if overlap, overrides the current one and delete the right one
            max_address = max(obj_c.get_max_address(), obj_r.get_max_address())
            obj_c.size = max_address - obj_c.get_base_address()
            del self.list[right]

    # Insert the object into the list and merge if possible
    def insert(self, pm_object):
        # empty list
        if len(self.list) == 0:
            self.list.append(pm_object)
            return

        length = len(self.list)

        # put it into the leftmost and try merge
        if pm_object.get_base_address() <= self.list[0].get_base_address():
            index = 0
            self.list.insert(index, pm_object)
            self.try_merge(index)
            return

        # put it into the rightmost and try merge
        if pm_object.get_base_address() >= \
                                        self.list[length-1].get_base_address():
            index = length
            self.list.insert(index, pm_object)
            self.try_merge(index)
            return

        # binary search to get the insert position and try merge
        left = 0
        right = length - 1
        while left+1 < right:
            mid = left + int((right-left) / 2)
            if pm_object.get_base_address() <= \
                                            self.list[mid].get_base_address():
                right = mid
            else:
                left = mid
        self.list.insert(right, pm_object)
        self.try_merge(right)

# P(src) -hb-> W(tgt)
# src and tgt are both PMObjects
class WitcherBelief:
    def __init__(self, hb_src, hb_tgt):
        self.hb_src = hb_src
        self.hb_tgt = hb_tgt

    def __repr__(self):
        return "WitcherBelief(%s, %s)" % (self.hb_src, self.hb_tgt)

    def __eq__(self, other):
        if isinstance(other, WitcherBelief):
            return ((self.hb_src == other.hb_src) and \
                    (self.hb_tgt == other.hb_tgt))
        else:
            return False

    def __ne__(self, other):
        return (not self.__eq__(other))

    def __hash__(self):
        return hash(self.__repr__())

class WitcherBeliefDatabase:
    def __init__(self, witcher_ppdgs):
        self.witcher_ppdg_list = witcher_ppdgs.witcher_ppdg_list

        # Get all pm objects into a list
        # If two objects overlap, we will merge them
        self.pm_object_list = PMObjectList()
        self.init_pm_object_list()

        # Add nodes(PMObjects) into the graph
        self.belief_graph = nx.DiGraph()
        self.init_belief_graph()

        self.belief_set = set()
        self.critical_read_set = set()
        self.init_belief_and_critical_read_set()

        # a dict: key:store_id, val:store_obj
        # used for accelerating get_pm_object
        self.dict_store_to_obj = dict()

    # Get pm object from a store
    def get_pm_object(self, store):
        if store.id in self.dict_store_to_obj:
            return self.dict_store_to_obj[store.id]

        store_obj = self.pm_object_list.find(store.address, store.size)
        self.dict_store_to_obj[store.id] = store_obj
        return store_obj

    # check whether this store is a critical store
    def is_critical_store(self, store):
        store_obj = self.get_pm_object(store)
        # in case we give up some ppdg
        if store_obj == None:
            return False
        return store_obj.id in self.critical_read_set

    # Here we want to persist p_st and leave v_st volatile
    # which means we need to find a belief: v_st_obj -hb-> p_st_obj
    def violate_ordering_belief(self, p_st, v_st):
        p_st_obj = self.get_pm_object(p_st)
        v_st_obj = self.get_pm_object(v_st)
        # in case we give up some ppdg
        if p_st_obj == None or v_st_obj == None:
            return False

        v_st_obj_id = v_st_obj.id
        v_st_obj_succ_id_set = set(self.belief_graph.successors(v_st_obj_id))

        p_st_obj_id = p_st_obj.id
        return p_st_obj_id in v_st_obj_succ_id_set

    def init_pm_object_list(self):
        for witcher_ppdg in self.witcher_ppdg_list:
            graph = witcher_ppdg.witcher_graph.graph
            self.init_pm_object_list_from_graph(graph)
        getLogger().debug("PMObjectList:" + str(self.pm_object_list.list))

    def init_pm_object_list_from_graph(self, graph):
        nodes_data = graph.nodes.data()
        getLogger().debug("graph nodes data:" + str(nodes_data))
        for node_data in nodes_data:
            node_id = node_data[0]
            address = node_data[1]["address"]
            size = node_data[1]["size"]
            # in case size = 0 for memcpy memset memmove
            if size > 0:
                self.pm_object_list.insert(PMObject(address, size))

    # Add nodes into the graph, node id is the PMObjectList index
    def init_belief_graph(self):
        for i, pm_obj in enumerate(self.pm_object_list.list):
            pm_obj.id = i
            self.belief_graph.add_node(i)

    def init_belief_and_critical_read_set(self):
        for witcher_ppdg in self.witcher_ppdg_list:
            graph = witcher_ppdg.witcher_graph.graph
            self.init_belief_and_critical_read_set_from_graph(graph)
        getLogger().debug("belief_graph_edges:" + str(self.belief_graph.edges))
        for edge in self.belief_graph.edges:
            getLogger().debug("edge:" + str(edge) + \
                              str(self.belief_graph[edge[0]][edge[1]]))
        getLogger().debug("belief_set:" + str(self.belief_set))
        getLogger().debug("critical_read_set:" + str(self.critical_read_set))

    def init_belief_and_critical_read_set_from_graph(self, graph):
        for edge in graph.edges:
            self.init_belief_and_critical_read_set_from_edge(graph, edge)

    def init_belief_and_critical_read_set_from_edge(self, graph, edge):
        getLogger().debug("edge:" + str(edge))
        src_id = edge[0]
        src_node = graph.nodes[src_id]
        assert(src_id == src_node["id"])
        src_node_type = src_node["type"]
        getLogger().debug("src_node:" + str(src_node))
        getLogger().debug("src_node address: %x" % src_node["address"])
        # in case size = 0 for memcpy memset memmove
        if src_node["size"] == 0:
            return
        src_obj = self.pm_object_list.find(src_node["address"], \
                                           src_node["size"])
        getLogger().debug("src_obj:" + str(src_obj))

        tgt_id = edge[1]
        tgt_node = graph.nodes[tgt_id]
        assert(tgt_id == tgt_node["id"])
        tgt_node_type = tgt_node["type"]
        getLogger().debug("tgt_node:" + str(tgt_node))
        getLogger().debug("tgt_node address: %x" % tgt_node["address"])
        # in case size = 0 for memcpy memset memmove
        if tgt_node["size"] == 0:
            return
        tgt_obj = self.pm_object_list.find(tgt_node["address"], \
                                           tgt_node["size"])
        getLogger().debug("tgt_obj:" + str(tgt_obj))

        edge_type = graph[src_id][tgt_id]["type"]

        if edge_type == "data":
            self.analyze_data_edge(graph, src_node_type, src_obj, \
                                          tgt_node_type, tgt_obj)
        else:
            assert(edge_type == "ctrl")
            self.analyze_ctrl_edge(graph, src_id, src_node_type, src_obj, \
                                                  tgt_node_type, tgt_obj)
    # Analyze a data edge
    def analyze_data_edge(self, graph, src_node_type, src_obj, \
                                       tgt_node_type, tgt_obj):
        # if src(ST) -dd-> tgt(LD)
        if src_node_type == "Store" and tgt_node_type == "Load":
            # tgt -hb-> src
            belief = WitcherBelief(tgt_obj, src_obj)
            self.belief_set.add(belief)
            # add tgt into critical read set
            self.critical_read_set.add(tgt_obj.id)

            # counting belief frequency
            self.add_belief_graph_edge(tgt_obj.id, src_obj.id)

    def analyze_ctrl_edge(self, graph, src_id, src_node_type, src_obj, \
                                               tgt_node_type, tgt_obj):
        # if src(ST) -cd-> tgt(LD)
        if src_node_type == "Store" and tgt_node_type == "Load":
            # tgt -hb-> src
            belief = WitcherBelief(tgt_obj, src_obj)
            self.belief_set.add(belief)
            # add tgt into critical read set
            self.critical_read_set.add(tgt_obj.id)

            # counting belief frequency
            self.add_belief_graph_edge(tgt_obj.id, src_obj.id)

        # if src(LD) -cd-> tgt(LD)
        if src_node_type == "Load" and tgt_node_type == "Load":
            # src -hb-> tgt
            belief = WitcherBelief(src_obj, tgt_obj)
            self.belief_set.add(belief)
            # add tgt into critical read set
            self.critical_read_set.add(tgt_obj.id)

            # counting belief frequency
            self.add_belief_graph_edge(src_obj.id, tgt_obj.id)

            # apply transitively
            ctrl_succesors = self.get_ctrl_succesors(graph, src_id)
            for pm_obj in ctrl_succesors:
                belief = WitcherBelief(pm_obj, tgt_obj)
                self.belief_set.add(belief)

                self.add_belief_graph_edge(pm_obj.id, tgt_obj.id)

    def get_ctrl_succesors(self, graph, src_id):
        ctrl_succesors = set()
        processed_id = set()
        queue = deque()
        queue.append(src_id)
        while len(queue) > 0:
            node_id = queue.popleft()
            succ_id_list = list(graph.predecessors(node_id))
            for succ_id in succ_id_list:
                if succ_id in processed_id:
                    continue
                processed_id.add(succ_id)

                succ_node = graph.nodes[succ_id]
                succ_node_type = succ_node["type"]

                if succ_node_type == "Store":
                    continue

                queue.append(succ_id)
                if succ_node["size"] == 0:
                    continue

                succ_obj = self.pm_object_list.find(succ_node["address"],\
                                                    succ_node["size"])
                ctrl_succesors.add(succ_obj)
        return ctrl_succesors

    def add_belief_graph_edge(self, prev_id, succ_id):
        if self.belief_graph.has_edge(prev_id, succ_id):
            self.belief_graph.edges[prev_id, succ_id]["count"] = \
                self.belief_graph.edges[prev_id, succ_id]["count"] + 1
        else:
            self.belief_graph.add_edge(prev_id, succ_id)
            self.belief_graph.edges[prev_id, succ_id]["count"] = 1
