//===-- sc - SAFECode Compiler Tool ---------------------------------------===//
//
//                     Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed
// under the University of Illinois Open Source License. See LICENSE.TXT for
// details.
//
//===----------------------------------------------------------------------===//
//
// This program is a tool to run the SAFECode passes on a bytecode input file.
//
//===----------------------------------------------------------------------===//

#include "Giri/TraceFile.h"
#include "Utility/StoreValueReader.h"

#include "llvm/Support/CommandLine.h"

#include <fcntl.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

static cl::opt<std::string>
TraceFilename(cl::Positional, cl::desc("trace file name"), cl::init("-"));

static cl::opt<std::string>
StoreFilename(cl::Positional, cl::desc("store value file name"), cl::init("-"));

static cl::opt<std::string>
PMAddr(cl::Positional, cl::desc("PM start address in hex"), cl::init("-"));

static cl::opt<std::string>
PMSize(cl::Positional, cl::desc("PM size in MiB"), cl::init("-"));

static cl::opt<std::string> ExtTracingFuncFilename(cl::Positional,
                                          cl::desc("External Tracing Function"),
                                          cl::init("-"));

// A file descriptor for trace file
int fd_trace = 0;
// A file descriptor for store value file
int fd_store = 0;
// PM start address
uintptr_t pm_addr_start = 0;
// PM end address
uintptr_t pm_addr_end = 0;
// A reader for store value file
StoreValueReader storeValueReader;
// A vector storing ext tracing functions
std::vector<std::string> ext_tracing_func_vector;

// clean up stuff
void cleanup() {
  close(fd_trace);
  storeValueReader.close();
  close(fd_store);
}

void processFenceEntry(Entry entry) {
  printf("Fence\n");
}

void processFlushEntry(Entry entry) {
  // check if it is inside the PM range
  if(entry.address < pm_addr_start || entry.address > pm_addr_end) {
    return;
  }

  printf("Flush,%lx\n", entry.address);
}

void processStoreEntry(Entry entry) {
  // check if it is inside the PM range
  if(entry.address < pm_addr_start || entry.address > pm_addr_end) {
    storeValueReader.moveCurrOffset(entry.length);
    return;
  }

  printf("Store,%lx,%lu,", entry.address, entry.length);

  // Some memcpy may use 0 length
  if (entry.length == 0) {
    return;
  }

  // get the store value through the storeValueReader
  unsigned char* val = (unsigned char*) malloc(entry.length);
  storeValueReader.getNextValue(val, entry.length);

  for(uintptr_t i = 0; i < entry.length - 1; ++i) {
    printf("%02x ", val[i]);
  }
  printf("%02x\n", val[entry.length - 1]);
  free(val);
}

// We used call for marking TX boundaries.
void processCallEntry(Entry entry) {
  // skip normal call
  if (entry.length == 0) {
    return;
  }

  unsigned func_index = entry.length - 1;
  std::string func_name = ext_tracing_func_vector[func_index];

  // TX start
  if (func_name == "witcher_tx_begin") {
    printf("TXStart\n");
    return;
  }

  // TX end
  if (func_name == "witcher_tx_end") {
    printf("TXEnd\n");
    return;
  }

  // pmdk tracing function
  printf("%s\n", func_name.c_str());
}

void run() {
  // a while loop for processing each entry
  Entry entry;
  ssize_t readsize;
  while ((readsize = read(fd_trace, &entry, sizeof(entry))) == sizeof(entry)) {
    switch (entry.type) {
      case RecordType::CLType:
        processCallEntry(entry);
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
      default:
        ;
    }

    // Stop printing entries if we've hit the end of the log.
    if (entry.type == RecordType::ENType) {
      readsize = 0;
      break;
    }
  }

  if (readsize != 0) {
    fprintf(stderr, "Read of incorrect size\n");
    exit(1);
  }
}

void parseExtTracingFunc() {
  assert((ExtTracingFuncFilename != "-") &&
         "Cannot open ext tracing func file!\n");

  std::ifstream file(ExtTracingFuncFilename);
  std::string str;
  while (std::getline(file, str)) {
    ext_tracing_func_vector.push_back(str);
  }
}

// Initial trace file, store value file, storeValueReader, PM address range
void init(int argc, char** argv) {
  // Parse the command line options.
  cl::ParseCommandLineOptions(argc, argv, "Print Trace Utility\n");

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

  pm_addr_start = std::stoul(PMAddr.c_str(), 0, 16);
  uintptr_t pm_size = std::stoul(PMSize.c_str(), 0, 10);
  pm_addr_end = pm_addr_start + pm_size * 1024 * 1024;

  parseExtTracingFunc();
}

int main(int argc, char** argv) {
  init(argc, argv);
  run();
  cleanup();

  return 0;
}
