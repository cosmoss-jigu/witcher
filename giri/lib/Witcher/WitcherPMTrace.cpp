#define DEBUG_TYPE "witcherpmtrace"

#include "Witcher/WitcherPMTrace.h"
#include "Utility/SourceLineMapping.h"

#include "llvm/Support/CommandLine.h"

#include <fcntl.h>
#include <iomanip>

using namespace witcher;

//===----------------------------------------------------------------------===//
//                        Command Line Arguments
//===----------------------------------------------------------------------===//

extern cl::opt<std::string> TraceFilename;

cl::opt<std::string>
StoreFilename("trace-store-file", cl::desc("store value file name"), cl::init("-"));

extern cl::opt<std::string> PMAddr;

extern cl::opt<std::string> PMSize;

extern cl::opt<std::string> ExtTracingFuncFilename;

static cl::opt<std::string>
PMTraceFilename("pmtrace-file", cl::desc("Output PM Trace File"), cl::init("-"));


//===----------------------------------------------------------------------===//
//                        WitcherPMTrace Implementations
//===----------------------------------------------------------------------===//
// ID Variable to identify the pass
char WitcherPMTrace::ID = 0;

// Pass registration
static RegisterPass<WitcherPMTrace> X("dwitcherpmtrace",
                                      "Persistent Trace Generation");

// clean up stuff
void WitcherPMTrace::cleanup() {
  close(fd_trace);
  storeValueReader.close();
  close(fd_store);
  pmtrace_of.close();
}

bool WitcherPMTrace::is_pm_addr(uintptr_t addr) {
  if(addr >= pm_addr_start && addr < pm_addr_end) {
    return true;
  }

  if(addr >= pm_addr_start_dup && addr < pm_addr_end_dup) {
    return true;
  }
  return false;
}

void WitcherPMTrace::processMmapEntry(Entry entry) {
  uintptr_t addr = entry.address;
  uintptr_t len = entry.length;

  if (addr == 18446744073709551615) {
    return;
  }

  if (addr == pm_addr_start) {
    return;
  }

  assert(pm_addr_start_dup == 0);
  assert(pm_addr_end_dup == 0);

  pm_addr_start_dup = addr;
  pm_addr_end_dup = addr + len;
}

void WitcherPMTrace::processTXAllocEntry(Entry entry) {
  pmtrace_of << "TXAlloc," << std::dec << entry.tid << ","
                           << std::hex << entry.address << ","
                           << std::dec << entry.length;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << std::endl;
}

void WitcherPMTrace::processTXAddEntry(Entry entry) {
  pmtrace_of << "TXAdd," << std::dec << entry.tid << ","
                         << std::hex << entry.address << ","
                         << std::dec << entry.length;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << std::endl;
}

void WitcherPMTrace::processFenceEntry(Entry entry) {
  pmtrace_of << "Fence," << std::dec << entry.tid;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << std::endl;
}

void WitcherPMTrace::processFlushEntry(Entry entry) {
  // check if it is inside the PM range
  if (!is_pm_addr(entry.address)) {
    return;
  }

  uintptr_t addr = entry.address;
  if(addr >= pm_addr_start_dup && addr < pm_addr_end_dup) {
    addr = addr - pm_addr_start_dup + pm_addr_start;
  }

  pmtrace_of << "Flush," << std::dec << entry.tid << ","
                         << std::hex << addr;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << std::endl;
}

void WitcherPMTrace::processStoreEntry(Entry entry) {
  // check if it is inside the PM range
  if (!is_pm_addr(entry.address)) {
    storeValueReader.moveCurrOffset(entry.length);
    return;
  }

  // Some memcpy may use 0 length
  if (entry.length == 0) {
    return;
  }

  uintptr_t addr = entry.address;
  if(addr >= pm_addr_start_dup && addr < pm_addr_end_dup) {
    addr = addr - pm_addr_start_dup + pm_addr_start;
  }

  pmtrace_of << "Store," << std::dec << entry.tid << ","
                         << std::hex << addr << ","
                         << std::dec << entry.length << ",";

  // get the store value through the storeValueReader
  unsigned char* val = (unsigned char*) malloc(entry.length);
  storeValueReader.getNextValue(val, entry.length);

  for(uintptr_t i = 0; i < entry.length - 1; ++i) {
    pmtrace_of << std::hex << std::setfill('0') << std::setw(2)
               << (int)val[i] << " ";
  }
  pmtrace_of << std::hex << std::setfill('0') << std::setw(2)
             << (int)val[entry.length-1];

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << std::endl;
  free(val);
}

void WitcherPMTrace::processRetEntry(Entry entry) {
  // skip normal call
  if (entry.length == 0) {
    return;
  }

  unsigned func_index = entry.length - 1;
  std::string func_name = ext_tracing_func_vector[func_index];

  // TX start
  if (func_name == "witcher_tx_begin" || func_name == "witcher_tx_end") {
    return;
  }

  // pmdk tracing function
  pmtrace_of << func_name << "," << std::dec << entry.tid;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << ",end" << std::endl;
}

// We used call for marking TX boundaries.
void WitcherPMTrace::processCallEntry(Entry entry) {
  // skip normal call
  if (entry.length == 0) {
    return;
  }

  unsigned func_index = entry.length - 1;
  std::string func_name = ext_tracing_func_vector[func_index];

  // TX start
  if (func_name == "witcher_tx_begin") {
    pmtrace_of << "TXStart," << std::dec << entry.tid;

    // print src info
    Instruction* I = lsNumPass->getInstByID(entry.id);
    std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
    pmtrace_of << "," << srcInfo << std::endl;
    return;
  }

  // TX end
  if (func_name == "witcher_tx_end") {
    pmtrace_of << "TXEnd," << std::dec << entry.tid;

    // print src info
    Instruction* I = lsNumPass->getInstByID(entry.id);
    std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
    pmtrace_of << "," << srcInfo << std::endl;
    return;
  }

  // pmdk tracing function
  pmtrace_of << func_name << "," << std::dec << entry.tid;

  // print src info
  Instruction* I = lsNumPass->getInstByID(entry.id);
  std::string srcInfo = SourceLineMappingPass::locateSrcInfo(I);
  pmtrace_of << "," << srcInfo << ",start" << std::endl;
}

void WitcherPMTrace::run() {
  // a while loop for processing each entry
  Entry entry;
  ssize_t readsize;
  while ((readsize = read(fd_trace, &entry, sizeof(entry))) == sizeof(entry)) {
    switch (entry.type) {
      case RecordType::CLType:
        processCallEntry(entry);
        break;
      case RecordType::RTType:
        processRetEntry(entry);
        break;
      case RecordType::STType:
        processStoreEntry(entry);
        break;
      case RecordType::FLType:
        processFlushEntry(entry);
        break;
      case RecordType::FEType:
        processFenceEntry(entry);
        break;
      case RecordType::TXADDType:
        processTXAddEntry(entry);
        break;
      case RecordType::TXALLOCType:
        processTXAllocEntry(entry);
        break;
      case RecordType::MmapType:
        processMmapEntry(entry);
      default:
        ;
    }

    // Stop printing entries if we've hit the end of the log.
    if (entry.type == RecordType::ENType) {
      readsize = 0;
      break;
    }

    // memecached doesn't have RecordType::ENType
    if (int(entry.type) == 0 && entry.id == 0 && entry.tid == 0 &&
          entry.address == 0 && entry.length == 0) {
      readsize = 0;
      break;
    }
  }

  if (readsize != 0) {
    fprintf(stderr, "Read of incorrect size\n");
    exit(1);
  }
}

void WitcherPMTrace::parseExtTracingFunc() {
  assert((ExtTracingFuncFilename != "-") &&
         "Cannot open ext tracing func file!\n");

  std::ifstream file(ExtTracingFuncFilename);
  std::string str;
  while (std::getline(file, str)) {
    ext_tracing_func_vector.push_back(str);
  }
}

void WitcherPMTrace::init() {
  // Open the trace file for read-only access.
  if (TraceFilename == "-")
    fd_trace = STDIN_FILENO;
  else
    fd_trace = open (TraceFilename.c_str(), O_RDONLY);
  assert((fd_trace != -1) && "Cannot open trace file!\n");

  // Open the store value file for read-only access.
  if (StoreFilename == "-")
    fd_store = STDIN_FILENO;
  else
    fd_store = open (StoreFilename.c_str(), O_RDONLY);
  assert((fd_store != -1) && "Cannot open store value file!\n");

  // Initialize the reader
  storeValueReader.init(fd_store);

  // init vars
  pm_addr_start = std::stoul(PMAddr.c_str(), 0, 16);
  uintptr_t pm_size = std::stoul(PMSize.c_str(), 0, 10);
  pm_addr_end = pm_addr_start + pm_size * 1024 * 1024;

  // get all ext tracing functions
  parseExtTracingFunc();

  // init output file stream
  pmtrace_of.open(PMTraceFilename);

  // get the QueryLoadStoreNumbers for instruction ID
  lsNumPass = &getAnalysis<QueryLoadStoreNumbers>();
}

bool WitcherPMTrace::runOnModule(Module &M) {
  init();
  run();
  cleanup();
  // This is an analysis pass, so always return false.
  return false;
}
