#define DEBUG_TYPE "witcherppdg"

#include "Witcher/WitcherParallel.h"
#include "Utility/SourceLineMapping.h"
#include "Utility/Debug.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

using namespace witcher;

//===----------------------------------------------------------------------===//
//                        Command Line Arguments
//===----------------------------------------------------------------------===//
extern cl::opt<std::string> PPDGFilename;

extern cl::opt<std::string> PMAddr;

extern cl::opt<std::string> PMSize;

//===----------------------------------------------------------------------===//
//                        WitcherPPDG Implementations
//===----------------------------------------------------------------------===//
// ID Variable to identify the pass
char WitcherParallelPPDG::ID = 0;

// Pass registration
static RegisterPass<WitcherParallelPPDG> X(
"dwitcherparallelppdg", "Dynamic Persistent Program Dependence Graph");

void WitcherParallelPPDG::printPPDG() {
  // Initialize the ppdg file
  std::error_code errinfo;
  raw_fd_ostream PPDGFile(PPDGFilename.c_str(),
                         errinfo,
                         sys::fs::OF_Append);
  if (errinfo) {
    errs() << "Error opening the pdg output file: " << PPDGFilename
           << " : " << errinfo.value() << "\n";
    return;
  }

  // call ppdg to write using dot format
  ppdg->write_graphviz(PPDGFile);
}

void WitcherParallelPPDG::getDataEdges(PDG* pdg,
                               PPDG* ppdg,
                               vertex_t pdg_v,
                               vertex_t ppdg_v,
                               Edge EdgeType,
                               unordered_set<vertex_t> &pdg_vertex_processed) {
  // The pdg_v should not be processed
  assert(pdg_vertex_processed.find(pdg_v) == pdg_vertex_processed.end());
  pdg_vertex_processed.insert(pdg_v);

  // If val(pdg_v) != val(ppdg_v) and ppdg_v contains val(pdg_v),
  // this means that they are two different persistent vertices,
  // so we can add the Persistent Dependence Edge and stop DFS.
  if(ppdg->contains(pdg->getVertexValue(pdg_v)) &&
          !(*pdg->getVertexValue(pdg_v) == *ppdg->getVertexValue(ppdg_v))) {
    set<vertex_t> ppdg_v_target_set = ppdg->getVertices(pdg->getVertexValue(pdg_v));
    if(EdgeType == Edge::DataDep) {
      for (vertex_t ppdg_v_target : ppdg_v_target_set) {
        ppdg->addDataDepEdge(ppdg_v, ppdg_v_target);
      }
    } else {
      for (vertex_t ppdg_v_target : ppdg_v_target_set) {
        ppdg->addCtrlDepEdge(ppdg_v, ppdg_v_target);
      }
    }
    return;
  }

  std::pair<out_edge_iterator, out_edge_iterator>
                                              pdgEdgeIt = pdg->out_edges(pdg_v);
  // Traverse the data dependence edges in pdg
  for (; pdgEdgeIt.first != pdgEdgeIt.second; ++pdgEdgeIt.first) {
    edge_t pdg_e = *pdgEdgeIt.first;
    if (!pdg->isDataDepEdge(pdg_e)){
      continue;
    }
    vertex_t pdg_v_target = pdg->target(pdg_e);

    // Only process pdg_v_target which has not been processed
    if (pdg_vertex_processed.find(pdg_v_target) != pdg_vertex_processed.end()) {
      continue;
    }

    // DFS call
    getDataEdges(pdg, ppdg, pdg_v_target, ppdg_v, EdgeType, pdg_vertex_processed);
  }
}

// TODO: control dependence transitive
void WitcherParallelPPDG::getCtrlEdges(PDG* pdg,
                               PPDG* ppdg,
                               vertex_t pdg_v,
                               vertex_t ppdg_v) {
  std::pair<out_edge_iterator, out_edge_iterator>
                                              pdgEdgeIt = pdg->out_edges(pdg_v);
  // Traverse the control dependence edges in pdg
  for (; pdgEdgeIt.first != pdgEdgeIt.second; ++pdgEdgeIt.first) {
    edge_t pdg_e = *pdgEdgeIt.first;
    if (!pdg->isCtrlDepEdge(pdg_e)){
      continue;
    }
    vertex_t pdg_v_target = pdg->target(pdg_e);
    // Then call DataEdges DFS
    unordered_set<vertex_t> pdg_vertex_processed;
    getDataEdges(pdg, ppdg, pdg_v_target, ppdg_v, Edge::CtrlDep, pdg_vertex_processed);
  }
}

void WitcherParallelPPDG::generaeteEdges(PDG* pdg,
                                 PPDG* ppdg) {
  std::pair<vertex_iter, vertex_iter> ppdgVtxIt = ppdg->vertices();
  // Traverse all the vertices in the ppdg and
  // get data and control dependence edges
  for (; ppdgVtxIt.first != ppdgVtxIt.second; ++ppdgVtxIt.first) {
    vertex_t ppdg_v = *ppdgVtxIt.first;
    DynValue* val = ppdg->getVertexValue(ppdg_v);

    assert(pdg->contains(val));
    vertex_t pdg_v = pdg->getVertexOrCreate(val);

    unordered_set<vertex_t> pdg_vertex_processed;
    getDataEdges(pdg, ppdg, pdg_v, ppdg_v, Edge::DataDep, pdg_vertex_processed);
    getCtrlEdges(pdg, ppdg, pdg_v, ppdg_v);
  }

}

void WitcherParallelPPDG::generateVertices(
                        PDG* pdg,
                        PPDG* ppdg,
                        unordered_map<DynValue, unsigned long>& valToIndexMap) {
  unsigned long indexStart = 0;
  unsigned long indexEnd = traceFile->getMaxIndex();

  // scan the trace within this TX from start to end
  unsigned long index = indexStart;
  for(; index <= indexEnd; ++index) {
    Entry entry = trace[index];
    // ignore entries which are not ST nor LD
    if(entry.type != RecordType::STType &&
        entry.type != RecordType::LDType) {
      continue;
    }

    // ignore ST and LD whose address is not inside PM address range
    if(!PMAddrRange.isInRange(entry.address)) {
      continue;
    }

    // get the instruction and its DynValue
    Instruction* I = lsNumPass->getInstByID(trace[index].id);
    DynValue* dynValue = traceFile->getDynValueFromIndex(I, index);

    // the pdg should contain but the ppdg should not contain this value
    // assert(pdg->contains(dynValue));
    // TODO if the ST/LD is not inside the data structure APIs, then the pdg
    // will not have it, because pdg uses BB index for range checking
    if (!pdg->contains(dynValue)) {
      DEBUG(dbgs() << "ppdg::generateVertices fails for index=" << index << "\n");
      continue;
    }

    // Set the trace index, entry and source code information for this node
    TraceInfo traceInfo = TraceInfo(index,
                                    entry,
                                    SourceLineMappingPass::locateSrcInfo(I));

    // add the value into the ppdg
    ppdg->createVertex(dynValue, traceInfo);

    // record the dynValue with it trace index
    valToIndexMap.insert(pair<DynValue, unsigned long>(*dynValue, index));

    DEBUG(dbgs() << "ppdg::generateVertices: index=" << index << "\n");
  }

}

void WitcherParallelPPDG::generatePPDG() {
  // generate vertices for this ppdg
  unordered_map<DynValue, unsigned long> valToIndexMap;
  generateVertices(pdg, ppdg, valToIndexMap);

  // generate edges for this ppdg
  generaeteEdges(pdg, ppdg);
}

void WitcherParallelPPDG::init() {
  // Initialize the graphs and the trace file.
  WitcherParallelPDG* witcherPDG = &getAnalysis<WitcherParallelPDG>();
  pdg = witcherPDG->getPDG();
  traceFile = witcherPDG->getTrace();
  trace = traceFile->getTrace();

  // Initialize the PM address range
  uintptr_t pm_addr_start = std::stoul(PMAddr.c_str(), 0, 16);
  uintptr_t pm_size = std::stoul(PMSize.c_str(), 0, 10);
  uintptr_t pm_addr_end = pm_addr_start + pm_size * 1024 * 1024;
  PMAddrRange = AddrRange(pm_addr_start, pm_addr_end);
  DEBUG(dbgs() << "PMAddrRange: start=" << PMAddrRange.getStart()
               << ", end=" << PMAddrRange.getEnd() << "\n");

  // get the QueryLoadStoreNumbers for instruction ID
  lsNumPass = &getAnalysis<QueryLoadStoreNumbers>();

  // init the ppdg
  ppdg = new PPDG();
}

bool WitcherParallelPPDG::runOnModule(Module &M) {
  init();
  generatePPDG();
  printPPDG();
  // This is an analysis pass, so always return false.
  return false;
}
