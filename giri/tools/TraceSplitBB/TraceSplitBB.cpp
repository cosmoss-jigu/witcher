#include "Giri/TraceFile.h"

#include "llvm/Support/CommandLine.h"

#include <fcntl.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <filesystem>
namespace fs = std::filesystem;

static cl::opt<std::string>
InputPath(cl::Positional, cl::desc("input path"), cl::init("-"));

static cl::opt<std::string>
OutputPath(cl::Positional, cl::desc("output path"), cl::init("-"));

void process_split_trace(const char* split_trace_name) {
  std::string input_file = InputPath + "/" + split_trace_name;
  int fd_input = open(input_file.c_str(), O_RDONLY);
  assert((fd_input != -1) && "Cannot open input file!\n");

  std::string output_file = OutputPath + "/" + split_trace_name;
  FILE* f_output = fopen(output_file.c_str(), "w+");
  assert((f_output != NULL) && "Cannot open output file!\n");

  // a while loop for processing each entry
  Entry entry;
  size_t readsize;
  while ((readsize = read(fd_input, &entry, sizeof(entry))) == sizeof(entry)) {
    // Stop printing entries if we've hit the end of the log.
    if (entry.type == RecordType::ENType) {
      readsize = 0;
      break;
    }

    // Print BB id
    if (entry.type == RecordType::BBType) {
      fprintf(f_output, "%d\n", entry.id);
      continue;
    }
  }

  if (readsize != 0) {
    fprintf(stderr, "Read of incorrect size\n");
    exit(1);
  }

  close(fd_input);
  fclose(f_output);
}

int main(int argc, char ** argv) {
  // Parse the command line options.
  cl::ParseCommandLineOptions(argc, argv, "Trace Split BB\n");

  // process each split_trace
  for(auto& p: fs::directory_iterator(InputPath.c_str())) {
    std::string split_trace_name = p.path().stem().string();
    process_split_trace(split_trace_name.c_str());
  }

  return 0;
}
