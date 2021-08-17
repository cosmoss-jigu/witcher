#ifndef WITCHER_H
#define WITCHER_H

#include "Witcher/ProgramDependenceGraph.h"
#include "Giri/TraceFile.h"
#include "Utility/BasicBlockNumbering.h"
#include "Utility/LoadStoreNumbering.h"
#include "Utility/PostDominanceFrontier.h"

#include "llvm/Pass.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"

using namespace giri;
using namespace std;

namespace witcher {

class WitcherPDG : public ModulePass {
public:
  static char ID;
  WitcherPDG() : ModulePass(ID) {}

  /// Using trace information, construct a dynamic program dependence graph
  /// \return false - The module was not modified.
  virtual bool runOnModule(Module &M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredTransitive<QueryBasicBlockNumbers>();
    AU.addRequiredTransitive<QueryLoadStoreNumbers>();

    // AU.addRequired<PostDominatorTree>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<PostDominanceFrontier>();

    // This pass is an analysis pass, so it does not modify anything
    AU.setPreservesAll();
  };

  // Pass the graphs to PPDG pass
  list<PDG*>& getGraphs() {
    return graphs;
  }

  // Pass the trace file to PPDG pass
  TraceFile* getTrace() {
    return Trace;
  }

private:
  // Initialize the pass by getting related passes and trace file
  void init();

  // Generate PDG
  void generatePDGs();

  // Pring PDG
  void printPDGs();

  // Get slicing result from initial, cache its result,
  // add both control and data dependent edges into the PDG
  void slicing(DynValue* initial,
               IndexRange curr_range,
               PDG* graph,
               unordered_set<DynValue> &processedValues,
               unordered_map<DynBasicBlock, DynValue*> &processedBBs);

  // Get the last control dependent DynValue (0 or 1)
  // Cache result, add control dependent edge, and add dep to the toProcess Q
  void slicingCtrlDep(DynValue* DV,
                      PDG* graph,
                      unordered_map<DynBasicBlock, DynValue*> &processedBBs,
                      deque<DynValue *> &toProcess);

  // Get the last data dependent DynValue(s) (0, 1 or more that 1)
  // add control dependent edge(s), and add dep(s) to the toProcess Q
  void slicingDataDep(DynValue* DV,
                      PDG* graph,
                      deque<DynValue*> &toProcess);

  // Copied from GIRI, used for control dependence slicing
  /// Find the basic blocks that can force execution of the specified basic
  /// block and return the identifiers used to represent those basic blocks
  /// within the dynamic trace.
  ///
  /// Note that this is slightly different from control-dependence.  A basic
  /// block can be forced to execute by a basic block on which it is
  /// control-dependent.  However, it can also be forced to execute simply
  /// because its containing function is executed (i.e., it post-dominates the
  /// entry block).
  ///
  /// \param[in] BB - The basic block for which the caller wants to know which
  ///                 basic blocks can force its execution.
  /// \param[out] bbNums - A set of basic block identifiers that can force
  ///                      execution of the specified basic block. Note that
  ///                      identifiers are *added* to the set.
  /// \return true  - The specified basic block will be executed at least once
  ///                 every time the function is called.
  /// \return false - The specified basic block may not be executed when the
  ///                 function is called (i.e., the specified basic block is
  ///                 control-dependent on the entry block if the entry block
  ///                 is in bbNums).
  bool findExecForcers(BasicBlock *BB, std::set<unsigned> &bbNums);

private:
  /// Graph for each TX
  list<PDG*> graphs;

  /// Trace file object (used for querying the trace)
  TraceFile *Trace;

  /// Cache of basic blocks that force execution of other basic blocks
  std::map<BasicBlock *, std::vector<BasicBlock *> > ForceExecCache;
  std::map<BasicBlock *, bool> ForceAtLeastOnceCache;

  /// Passes used by this pass
  const QueryBasicBlockNumbers *bbNumPass;
  const QueryLoadStoreNumbers *lsNumPass;
};

class WitcherPPDG : public ModulePass {
public:
  static char ID;
  WitcherPPDG() : ModulePass(ID) {}

  /// Convert PDGs to PPDGs
  /// \return false - The module was not modified.
  virtual bool runOnModule(Module &M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredTransitive<QueryLoadStoreNumbers>();
    AU.addRequiredTransitive<WitcherPDG>();

    // This pass is an analysis pass, so it does not modify anything
    AU.setPreservesAll();
  };

private:
  // Initialize the pass by getting related resources from the PDG pass
  void init();
  // Generate PPDG for each TX
  void generatePPDGs();
  // Pring PPDG
  void printPPDGs();
  // Get all the persistent vertices from pdg and insert them to ppdg
  void generateVertices(PDG* pdg,
                        PPDG* ppdg,
                        IndexRange txRange,
                        unordered_map<DynValue, unsigned long>& valToIndexMap);
  // Add persistent dependence edges
  void generaeteEdges(PDG* pdg,
                      PPDG* ppdg);
  // Use trace index for each Persistent vertices
  void vertexUseTraceIndex(
                        PPDG* ppdg,
                        unordered_map<DynValue, unsigned long>& valToIndexMap);
  // A DFS for getting Data Dependence Edges
  void getDataEdges(PDG* pdg,
                    PPDG* ppdg,
                    vertex_t pdg_v,
                    vertex_t ppdg_v,
                    Edge EdgeType,
                    unordered_set<vertex_t> &pdg_vertex_processed);
  // A DFS for getting Control Dependence Edges
  void getCtrlEdges(PDG* pdg,
                    PPDG* ppdg,
                    vertex_t pdg_v,
                    vertex_t ppdg_v);

private:
  /// Graph for each TX
  list<PDG*> pdgs;
  list<PPDG*> ppdgs;

  /// Trace file object (used for querying the trace)
  TraceFile *traceFile;
  /// trace of recorded entries got from trace file
  Entry* trace;
  /// TX ranges got from trace file
  std::list<IndexRange> txRanges;

  /// Address range of PM
  AddrRange PMAddrRange;

  /// Passes used by this pass
  const QueryLoadStoreNumbers *lsNumPass;
};

}

#endif
