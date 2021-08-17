#define DEBUG_TYPE "witcherpdg"

#include "Witcher/WitcherParallel.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/FileSystem.h"

using namespace witcher;

//===----------------------------------------------------------------------===//
//                        Command Line Arguments
//===----------------------------------------------------------------------===//
// The trace filename was specified externally in tracing part
extern cl::opt<std::string> TraceFilename;

//static cl::opt<std::string>
//PDGFilename("pdg-file", cl::desc("PDG output file name"), cl::init("-"));

extern cl::opt<std::string> PDGFilename;

//===----------------------------------------------------------------------===//
//                        WitcherPDG Pass Statistics
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//                        WitcherPDG Implementations
//===----------------------------------------------------------------------===//
// ID Variable to identify the pass
char WitcherParallelPDG::ID = 0;

// Pass registration
static RegisterPass<WitcherParallelPDG> X(
"dwitcherparallelpdg", "Dynamic Program Dependence Graph in Parallel");

void WitcherParallelPDG::init() {
  // Get references to other passes used by this pass.
  bbNumPass = &getAnalysis<QueryBasicBlockNumbers>();
  lsNumPass = &getAnalysis<QueryLoadStoreNumbers>();

  // Open the trace file and get ready to start using it.
  Trace = new TraceFile(TraceFilename, bbNumPass, lsNumPass, false);

  // Init the pdg
  pdg = new PDG();
}

void WitcherParallelPDG::slicingDataDep(DynValue* DV,
                                PDG* graph,
                                deque<DynValue*> &toProcess) {
  deque<DynValue*> dataDeps;
  Trace->getSourcesFor(*DV, dataDeps);

  for (DynValue* dataDep : dataDeps) {
    // Add control Dependence edge
    graph->addDataDepEdge(DV, dataDep);
    // Add the dep to the toProcess Queue
    toProcess.push_front(dataDep);
  }
}

bool WitcherParallelPDG::findExecForcers(BasicBlock *BB,
                                  std::set<unsigned> &bbNums) {
  // Get the parent function containing this basic block.  We'll need it for
  // several operations.
  Function *F = BB->getParent();

  // If we have already determined which basic blocks force execution of the
  // specified basic block, determine the IDs of these basic blocks and return
  // them.
  if (ForceExecCache.find(BB) != ForceExecCache.end()) {
    // Convert the basic blocks forcing execution into basic block ID numbers.
    for (unsigned index = 0; index < ForceExecCache[BB].size(); ++index) {
      BasicBlock *ForcerBB = ForceExecCache[BB][index];
      bbNums.insert(bbNumPass->getID(ForcerBB));
    }

    // Determine if the entry basic block forces execution of the specified
    // basic block.
    return ForceAtLeastOnceCache[BB];
  }

  // Otherwise, we need to determine which basic blocks force the execution of
  // the specified basic block.  We'll first need to grab the post-dominance
  // frontier and post-dominance tree for the entire function.
  //
  // Note: As of LLVM 2.6, the post-dominance analyses below will get executed
  //       every time we request them, so only ask for them once per function.
  //
  PostDominanceFrontier &PDF = getAnalysis<PostDominanceFrontier>(*F);
  //PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(*F);
  PostDominatorTree &PDT = getAnalysis<PostDominatorTreeWrapperPass>(*F).getPostDomTree();

  // Find which basic blocks force execution of each basic block within the
  // function.  Record the results for future use.
  for (Function::iterator bb = F->begin(); bb != F->end(); ++bb) {
    // Find all of the basic blocks on which this basic block is
    // control-dependent.  Record these blocks as they can force execution.
    PostDominanceFrontier::iterator i = PDF.find(&*bb);
    if (i != PDF.end()) {
      PostDominanceFrontier::DomSetType &CDSet = i->second;
      std::vector<BasicBlock *> &ForceExecSet = ForceExecCache[&*bb];
      ForceExecSet.insert(ForceExecSet.end(), CDSet.begin(), CDSet.end());
    }

    // If the specified basic block post-dominates the entry block, then we
    // know it will be executed at least once every time the function is called.
    // Therefore, execution of the entry block forces execution of the basic
    // block.
    BasicBlock &entryBlock = F->getEntryBlock();
    if (PDT.properlyDominates(&*bb, &entryBlock)) {
      ForceExecCache[&*bb].push_back(&entryBlock);
      ForceAtLeastOnceCache[BB] = true;
    } else {
      ForceAtLeastOnceCache[BB] = false;
    }
  }

  // Now that we've updated the cache, call ourselves again to get the answer.
  return findExecForcers(BB, bbNums);
}

void WitcherParallelPDG::slicingCtrlDep
                      (DynValue* DV,
                       PDG* graph,
                       unordered_map<DynBasicBlock, DynValue*> &processedBBs,
                       deque<DynValue *> &toProcess) {
  // Get the dynamic basic block to which this value belongs.
  DynBasicBlock DBB = DynBasicBlock(*DV);

  // Do nothing if the BB is null
  if (DBB.isNull()) {
    return;
  }

  BasicBlock &entryBlock = DBB.getParent()->getEntryBlock();

  // If the basic block is the entry block, then don't do anything.  We
  // already know that it forced its own execution.
  if (DBB.getBasicBlock() == &entryBlock) {
    return;
  }

  // This basic block was not an entry basic block.  Insert it into the
  // set of processed elements; if it was not already processed, process
  // it now.
  auto it = processedBBs.find(DBB);
  if (it != processedBBs.end()) {
    // If it was processed before, use the processed result
    DynValue* ctrl_dep = it->second;
    if (ctrl_dep != NULL) {
      graph->addCtrlDepEdge(DV, ctrl_dep);
    }
  } else {
    // Okay, this is not an entry basic block, and it has not been
    // processed before.  Find the set of basic blocks that can force
    // execution of this basic block.
    std::set<unsigned> forcesExecSet;
    bool found = findExecForcers(DBB.getBasicBlock(), forcesExecSet);

    // Find the previously executed basic block which caused execution of
    // this basic block.
    DynBasicBlock Forcer = Trace->getExecForcer(DBB, forcesExecSet);

    // If the basic block that forced execution is the entry block, and
    // the basic block is not control-dependent on the entry block, then
    // no control dependence exists and nothing needs to be done.
    // Otherwise, add the condition of the basic block that forced
    // execution to the worklist.
    if (Forcer.getBasicBlock() == nullptr) { // Couldn't find CD
      errs() << "Could not find Control-dep of this Basic Block \n";
    } else if (Forcer.getBasicBlock() != &entryBlock || !found ||
    // TODO: if the entry BB is the forcer, and if the forcesExecSet only
    //       contains the entry BB???
              (forcesExecSet.size() == 1 &&
               forcesExecSet.find(bbNumPass->getID(Forcer.getBasicBlock())) !=
               forcesExecSet.end())) {
      DynValue DTerminator = Forcer.getTerminator();

      deque<DynValue*> ctrlDeps;
      Trace->addToWorklist(DTerminator, ctrlDeps, *DV);

      for (DynValue* ctrlDep : ctrlDeps) {
        // Cache the result
        processedBBs.insert({DBB, ctrlDep});
        // Add control Dependence edge
        graph->addCtrlDepEdge(DV, ctrlDep);
        // Add the dep to the toProcess Queue
        toProcess.push_front(ctrlDep);
      }
    } else {
      // If it comes here, it means it processed but got no deps, so we cache it
      processedBBs.insert({DBB, NULL});
    }
  }
}

void WitcherParallelPDG::slicing(DynValue* initial,
                         PDG* graph,
                         unordered_set<DynValue> &processedValues,
                         unordered_map<DynBasicBlock, DynValue*> &processedBBs) {
  // This queue buffers all values to be processed
  deque<DynValue *> toProcess;
  toProcess.push_back(initial);

  while(!toProcess.empty()) {
    DynValue *DV = toProcess.front();
    toProcess.pop_front();

    // Normalize the dynamic value.
    Trace->normalize(*DV);

    // Check to see if this dynamic value has already been processed.
    if (processedValues.find(*DV) != processedValues.end()) {
      continue;
    }

    // Mark this value has been processed
    processedValues.insert(*DV);

    // Control Dependence Analysis
    slicingCtrlDep(DV, graph, processedBBs, toProcess);

    // Data Dependence Analysis
    slicingDataDep(DV, graph, toProcess);
  }
}

void WitcherParallelPDG::generatePDG() {
  PDG* graph = pdg;

  // Get the last Store or Load
  DynValue* curr_load_or_store = Trace->getNextLoadOrStore();

  // Store all processed Value
  unordered_set<DynValue> processedValues;
  // Store intermediate result of control dependence analysis
  unordered_map<DynBasicBlock, DynValue*> processedBBs;
  while (curr_load_or_store != NULL) {
    slicing(curr_load_or_store, graph, processedValues, processedBBs);
    curr_load_or_store = Trace->getNextLoadOrStore();
  }
}

void WitcherParallelPDG::printPDG() {
  // Initialize the pdg file
  std::error_code errinfo;
  raw_fd_ostream PDGFile(PDGFilename.c_str(),
                         errinfo,
                         sys::fs::OF_Append);

  if (errinfo) {
    errs() << "Error opening the pdg output file: " << PDGFilename
           << " : " << errinfo.value() << "\n";
    return;
  }
  // call pdg to write using dot format
  pdg->write_graphviz(PDGFile);
}

bool WitcherParallelPDG::runOnModule(Module &M) {
  init();
  generatePDG();
  printPDG();
  // This is an analysis pass, so always return false.
  return false;
}
