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

#include <cstdio>
#include <cassert>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static cl::opt<std::string>
TraceFilename(cl::Positional, cl::desc("trace file name"), cl::init("-"));

static cl::opt<std::string>
StoreFilename(cl::Positional, cl::desc("store value file name"), cl::init("-"));

int main(int argc, char ** argv) {
  // Parse the command line options.
  cl::ParseCommandLineOptions(argc, argv, "Print Trace Utility\n");

  // Open the trace file for read-only access.
  int fd_trace = 0;
  if (TraceFilename == "-")
    fd_trace = STDIN_FILENO;
  else
    fd_trace = open (TraceFilename.c_str(), O_RDONLY);
  assert((fd_trace != -1) && "Cannot open trace file!\n");

  // Open the store value file for read-only access.
  int fd_store = 0;
  if (StoreFilename == "-")
    fd_store = STDIN_FILENO;
  else
    fd_store = open (StoreFilename.c_str(), O_RDONLY);
  assert((fd_store != -1) && "Cannot open store value file!\n");

  // Initialize the reader
  StoreValueReader storeValueReader;
  storeValueReader.init(fd_store);

  // Print a header that reminds the user of what the fields mean.
  printf("-----------------------------------------------------------------------------\n");
  printf("%10s:  Record Type: %6s: %15s: %16s: %8s: %16s\n",
         "Index", "ID", "TID", "Address", "Length", "StoreValue");
  printf("-----------------------------------------------------------------------------\n");

  // Read in each entry and print it out.
  Entry entry;
  ssize_t readsize;
  unsigned index = 0;
  while ((readsize = read(fd_trace, &entry, sizeof(entry))) == sizeof(entry)) {
    printf("%10u: ", index++);

    // Print the entry's type
    switch (entry.type) {
      case RecordType::BBType:
        printf("BasicBlock  : ");
        break;
      case RecordType::LDType:
        printf("Load        : ");
        break;
      case RecordType::STType:
        printf("Store       : ");
        break;
      case RecordType::PDType:
        printf("Select      : ");
        break;
      case RecordType::CLType:
        printf("Call        : ");
        break;
      case RecordType::RTType:
        printf("Return      : ");
        break;
      case RecordType::ENType:
        printf("End         : ");
        break;
      case RecordType::FLType:
        printf("Flush       : ");
        break;
      case RecordType::FEType:
        printf("Fence       : ");
        break;
    }

    // Print the value associated with the entry.
    if (entry.type == RecordType::BBType)
      printf("%6u: %8lu: %16lx: %8lu\n",
             entry.id,
             entry.tid,
             entry.address,
             entry.length);
    else if (entry.type != RecordType::STType)
      printf("%6u: %8lu: %16lx: %8lx\n",
             entry.id,
             entry.tid,
             entry.address,
             entry.length);
    else {
      // print only for store here
      // get the store value from the reader
      printf("%6u: %8lu: %16lx: %8lx:      ",
             entry.id,
             entry.tid,
             entry.address,
             entry.length);

      // Some memcpy may use 0 length
      if (entry.length == 0) {
        continue;
      }

      unsigned char* val = (unsigned char*) malloc(entry.length);
      storeValueReader.getNextValue(val, entry.length);

      for(uintptr_t i = entry.length - 1; i > 0; i--) {
        printf("%X ", val[i]);
      }
      printf("%X\n", val[0]);
      free(val);
    }

    // Stop printing entries if we've hit the end of the log.
    if (entry.type == RecordType::ENType) {
      readsize = 0;
      break;
    }
  }

  // unma and close the fd
  storeValueReader.close();
  close(fd_store);

  if (readsize != 0) {
    fprintf(stderr, "Read of incorrect size\n");
    exit(1);
  }

  return 0;
}
