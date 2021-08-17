#ifndef WITCHERPMTRACE_H
#define WITCHERPMTRACE_H

#include "Giri/TraceFile.h"
#include "Utility/StoreValueReader.h"

#include <fstream>

namespace witcher {

class WitcherPMTrace: public ModulePass {
public:
  static char ID;
  WitcherPMTrace() : ModulePass(ID) {}

  /// \return false - The module was not modified.
  virtual bool runOnModule(Module &M);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredTransitive<QueryLoadStoreNumbers>();

    // This pass is an analysis pass, so it does not modify anything
    AU.setPreservesAll();
  };

private:
  void init();
  void run();
  void cleanup();

  void parseExtTracingFunc();
  void processCallEntry(Entry entry);
  void processRetEntry(Entry entry);
  void processStoreEntry(Entry entry);
  void processFlushEntry(Entry entry);
  void processFenceEntry(Entry entry);
  void processTXAddEntry(Entry entry);
  void processTXAllocEntry(Entry entry);
  void processMmapEntry(Entry entry);
  bool is_pm_addr(uintptr_t addr);

private:
  // A file descriptor for trace file
  int fd_trace = 0;
  // A file descriptor for store value file
  int fd_store = 0;
  // pmtrace output file stream
  std::ofstream pmtrace_of;
  // PM start address
  uintptr_t pm_addr_start = 0;
  uintptr_t pm_addr_start_dup = 0;
  // PM end address
  uintptr_t pm_addr_end = 0;
  uintptr_t pm_addr_end_dup = 0;
  // A reader for store value file
  StoreValueReader storeValueReader;
  // A vector storing ext tracing functions
  std::vector<std::string> ext_tracing_func_vector;

  /// Passes used by this pass
  const QueryLoadStoreNumbers *lsNumPass;
};

}

#endif
