#include "Giri/TraceFile.h"

#include "llvm/Support/CommandLine.h"

#include <fcntl.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>

static cl::opt<std::string>
TraceFilename(cl::Positional, cl::desc("trace file name"), cl::init("-"));

static cl::opt<std::string> ExtTracingFuncFilename(cl::Positional,
                                          cl::desc("External Tracing Function"),
                                          cl::init("-"));

static cl::opt<std::string>
OutputPath(cl::Positional, cl::desc("output path"), cl::init("-"));

// A file descriptor for trace file
int fd_trace = 0;
// A vector storing ext tracing functions
std::vector<std::string> ext_tracing_func_vector;

// tx markers
bool inside_tx = false;
int tx_start_to_skip = 0;
// tx index
int curr_tx_index = 0;
// fd for writing the trace of one tx
int fd_output = 0;

// cleanup stuff
void cleanup() {
  close(fd_trace);
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
    // mark inside_tx
    assert(inside_tx == false);
    inside_tx = true;
    // we need to skip the call and ret entry of TX_START
    tx_start_to_skip = 2;

    // open the file to write the trace of one tx
    assert(fd_output == 0);
    if (OutputPath == "-") {
      fd_output = STDIN_FILENO;
    } else {
      std::string output_file = OutputPath + "/" +
                                std::to_string(curr_tx_index);
      fd_output = open(output_file.c_str(), O_WRONLY | O_CREAT, 0640u);
    }
    assert((fd_output != -1) && "Cannot output file!\n");
    return;
  }

  // TX end
  if (func_name == "witcher_tx_end") {
    // write the END entry
    Entry entry = Entry(RecordType::ENType, 0);
    write(fd_output, &entry, sizeof(entry));

    // mark inside_tx
    assert(inside_tx == true);
    inside_tx = false;

    // close the fd
    close(fd_output);
    fd_output = 0;

    // inc the tx index
    curr_tx_index++;
    return;
  }
}

void run() {
  // a while loop for processing each entry
  Entry entry;
  ssize_t readsize;
  while ((readsize = read(fd_trace, &entry, sizeof(entry))) == sizeof(entry)) {
    if (entry.type == RecordType::CLType) {
      processCallEntry(entry);
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

    if (inside_tx) {
      if (tx_start_to_skip > 0) {
        // we need to skip the call and ret entry of TX_START
        tx_start_to_skip--;
      } else {
        // write the entry inside a tx
        write(fd_output, &entry, sizeof(entry));
      }
    }
  }

  if (readsize != 0) {
    fprintf(stderr, "Read of incorrect size\n");
    exit(1);
  }
}

// parse ext tracing functions
void parseExtTracingFunc() {
  assert((ExtTracingFuncFilename != "-") &&
         "Cannot open ext tracing func file!\n");

  std::ifstream file(ExtTracingFuncFilename);
  std::string str;
  while (std::getline(file, str)) {
    ext_tracing_func_vector.push_back(str);
  }
}

void init(int argc, char** argv) {
  // Parse the command line options.
  cl::ParseCommandLineOptions(argc, argv, "Trace Split\n");

  // Open the trace file for read-only access.
  if (TraceFilename == "-")
    fd_trace = STDIN_FILENO;
  else
    fd_trace = open(TraceFilename.c_str(), O_RDONLY);
  assert((fd_trace != -1) && "Cannot open trace file!\n");

  // parse ext tracing functions
  parseExtTracingFunc();
}

int main(int argc, char ** argv) {
  init(argc, argv);
  run();
  cleanup();

  return 0;
}
