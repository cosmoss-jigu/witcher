//===- Giri.h - Dynamic Slicing Pass ----------------------------*- C++ -*-===//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files implements the classes for reading the trace file.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giri"

#include "Giri/TraceFile.h"
#include "Utility/Debug.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"

#include <cassert>
#include <vector>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace giri;
using namespace llvm;
using namespace std;

//===----------------------------------------------------------------------===//
//                          Pass Statistics
//===----------------------------------------------------------------------===//
STATISTIC(NumStaticBuggyVal, "Num. of possible missing matched static values");
STATISTIC(NumDynBuggyVal, "Number of possible missing matched dynamic values");

//===----------------------------------------------------------------------===//
//                          Public TraceFile Interfaces
//===----------------------------------------------------------------------===//
TraceFile::TraceFile(string Filename,
                     const QueryBasicBlockNumbers *bbNums,
                     const QueryLoadStoreNumbers *lsNums,
                     bool init_tx) :
  bbNumPass(bbNums), lsNumPass(lsNums),
  trace(0), totalLoadsTraced(0), lostLoadsTraced(0) {
  // Open the trace file for read-only access.
  int fd = open(Filename.c_str(), O_RDONLY);
  assert((fd > 0) && "Cannot open file!\n");

  // Attempt to get the file size.
  struct stat finfo;
  int ret = fstat(fd, &finfo);
  assert((ret == 0) && "Cannot fstat() file!\n");
  // Calculate the index of the last record in the trace.
  maxIndex = finfo.st_size / sizeof(Entry) - 1;
  // Initialize currIndex using maxindex
  currIndex = maxIndex;

  // Note that we map the whole file in the private memory space. If we don't
  // have enough VM at this time, this will definitely fail.
  trace = (Entry *)mmap(0,
                        finfo.st_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE,
                        fd,
                        0);
  assert((trace != MAP_FAILED) && "Trace mmap() failed!\n");

  // TODO: this may override loads we are interested in, so for now we comment
  // it out.
  // Fixup lost loads.
  // fixupLostLoads();
  buildTraceFunAddrMap();

  // we don't need to init the tx for pdg parallel
  if (init_tx) {
    // Initial the TXs index ranges
    initTXSegment();
  }

  DEBUG(dbgs() << "TraceFile " << Filename << " successfully initialized.\n");
}

DynValue *TraceFile::getLastDynValue(Value  *V) {
  // Determine if this is an instruction. If not, then it is some other value
  // that doesn't belong to a specific basic block within the trace.
  Instruction *I = dyn_cast<Instruction>(V);
  if (I == nullptr)
    return new DynValue(V, 0);

  // First, get the ID of the basic block containing this instruction.
  unsigned id = bbNumPass->getID(I->getParent());
  assert(id && "Basic block does not have ID!\n");

  // Next, scan backwards through the trace (starting from the end) until we
  // find a matching basic block ID.
  for (unsigned long index = maxIndex; index > 0; --index) {
    if (trace[index].type == RecordType::BBType && trace[index].id == id)
      return new DynValue(I, index);
  }

  // If this is the first block, verify that it is the for the value for which
  // we seek.  If it isn't, then flag an error with an assertion.
  assert(trace[0].type == RecordType::BBType && trace[0].id == id &&
         "Cannot find instruction in trace!\n");

  return new DynValue(I, 0);
}

DynValue *TraceFile::getDynValueFromIndex(Instruction *I, unsigned long start) {
  // First, get the ID of the basic block containing this instruction.
  unsigned id = bbNumPass->getID(I->getParent());
  assert(id && "Basic block does not have ID!\n");

  // Next, scan forward to find the matching BB id
  // TODO: a stack for recursive call
  list<uintptr_t> stack;
  for (unsigned long index = start + 1; index < maxIndex; ++index) {
    if (trace[index].type == RecordType::CLType) {
      stack.push_front(trace[index].address);
      continue;
    }
    if (trace[index].type == RecordType::RTType) {
      if (stack.size() > 0 && stack.front() == trace[index].address) {
        stack.pop_front();
        continue;
      }
    }
    if (stack.size() > 0) {
      continue;
    }
    if (trace[index].type == RecordType::BBType && trace[index].id == id) {
      assert(trace[index].id == id &&
             "BB ID mismatch in in getDynValueFromIndex!\n");
      return new DynValue(I, index);
    }
  }

  errs() << "Cannot find a BB for an instruction!\n";
  return NULL;
}

DynValue* TraceFile::getNextLoadOrStore() {
  DynValue* ret = NULL;
  unsigned long index = currIndex;

  for (; index > 0; --index) {
    if (trace[index].type == RecordType::STType ||
            trace[index].type == RecordType::LDType) {
      Instruction* I = lsNumPass->getInstByID(trace[index].id);
      ret = getDynValueFromIndex(I, index);
      // this may happen in pdg parallel, so just skip it
      if (ret == NULL) {
        continue;
      }
      break;
    }
  }
  currIndex = index - 1;
  return ret;
}

unsigned long TraceFile::getIndexForTheLoadOrStore() {
  return currIndex + 1;
}

/// get the next TX range
/// [0, 0] mean all TXs have been returned
IndexRange TraceFile::getNextTXRange() {
  if (currTXs == 0) {
    return IndexRange(0, 0);
  }

  IndexRange currRange = txSegment.back();
  txSegment.pop_back();
  txSegment.push_front(currRange);
  currTXs--;

  return currRange;
}

std::list<IndexRange>& TraceFile::getTXRanges() {
  return txSegment;
}

unsigned long TraceFile::getMaxIndex() {
  return maxIndex;
}

Entry* TraceFile::getTrace() {
  return trace;
}

void TraceFile::getSourcesFor(DynValue &DInst, Worklist_t &Worklist) {
  if (BranchInst *BI = dyn_cast<BranchInst>(DInst.V)) {
    // If it is a conditional branch or switch instruction, add the conditional
    // value to to set of sources to backtrace.  If it is an unconditional
    // branch, do not add it to the slice at all as it will have no incoming
    // operands (other than Basic Blocks).
    if (BI->isConditional()) {
      DynValue NDV = DynValue(BI->getCondition(), DInst.index);
      addToWorklist(NDV, Worklist, DInst);
    }
  } else if (SwitchInst *SI = dyn_cast<SwitchInst>(DInst.V)) {
    DynValue NDV = DynValue(SI->getCondition(), DInst.index);
    addToWorklist(NDV, Worklist, DInst);
  } else if (isa<PHINode>(DInst.V)) {
    // If DV is an PHI node, we need to determine which predeccessor basic block
    // was executed.
    getSourcesForPHI(DInst, Worklist);
  } else if (isa<SelectInst>(DInst.V)) {
    // If this is a select instrution, we want to determine which of its inputs
    // we selected; the other input (and its def-use chain) can be ignored,
    // yielding a more accurate backwards slice.
    getSourceForSelect(DInst, Worklist);
  } else if (isa<Argument>(DInst.V)) {
    // If this is a function argument, handle it specially.
    getSourcesForArg(DInst, Worklist);
  } else if (LoadInst *LI = dyn_cast<LoadInst>(DInst.V)) {
    // If this is a load, add the address generating instruction to source
    // and find the corresponding store instruction.

    // The dereferenced pointer should be part of the dynamic backwards slice.
    DynValue NDV = DynValue(LI->getOperand(0), DInst.index);
    addToWorklist(NDV, Worklist, DInst);

    // Find the store instruction that generates that value that this load
    // instruction returns.
    getSourcesForLoad(DInst, Worklist);
  } else if (isa<CallInst>(DInst.V)) {
    // If it is a call instruction, do the appropriate tracing into the callee.
    if (!getSourcesForSpecialCall(DInst, Worklist))
      getSourcesForCall(DInst, Worklist);
  } else if (Instruction *I = dyn_cast<Instruction>(DInst.V)) {
    // We have exhausted all other possibilities, so this must be a regular
    // instruction.  We will create Dynamic Values for each of its input
    // operands, but we will use the *same* index into the trace.  The reason is
    // that all the operands dominate the instruction, so we know exactly which
    // blocks were executed by looking at the SSA graph in the LLVM IR.  We only
    // need to go searching through the trace when only a *subset* of the
    // operands really contributed to the computation.
    for (unsigned index = 0; index < I->getNumOperands(); ++index)
      if (!isa<Constant>(I->getOperand(index))) {
        DynValue NDV = DynValue(I->getOperand(index), DInst.index);
        addToWorklist(NDV, Worklist, DInst);
      }
  }

  // If the value isn't any of the above, then we assume it's a
  // terminal value (like a constant, global constant value like
  // function pointer) and that there are no more inputs into it.
}

DynBasicBlock TraceFile::getExecForcer(DynBasicBlock DBB,
                                       const set<unsigned> &bbnums) {
  // Normalize the dynamic basic block.
  if (!normalize(DBB))
    return DynBasicBlock(nullptr, maxIndex);

  // Find the execution of the basic block that forced execution of the
  // specified basic block.
  unsigned long index = findPreviousID(DBB.BB->getParent(),
                                       DBB.index - 1,
                                       RecordType::BBType,
                                       trace[DBB.index].tid,
                                       bbnums);

  if (index == maxIndex) // We did not find the record
    return DynBasicBlock(nullptr, maxIndex);

  // Assert that we have found the entry.
  assert(trace[index].type == RecordType::BBType);
  return DynBasicBlock(bbNumPass->getBlock(trace[index].id), index);
}

void TraceFile::addToWorklist(DynValue &DV,
                              Worklist_t &Sources,
                              DynValue &Parent) {
  // Allocate a new DynValue which can stay till deallocated
  DynValue *temp = new DynValue(DV);
  temp->setParent(&Parent); // Used for traversing data flow graph
  // @TODO Later make it generic to support both DFS & BFS
  Sources.push_front(temp);
}

bool TraceFile::normalize(DynBasicBlock &DBB) {
  if (BuggyValues.find(DBB.BB) != BuggyValues.end()) {
    NumDynBuggyVal++;
    return false; // Buggy value, likely to fail again
  }

  // Search for the basic block within the dynamic trace. Start with the
  // current entry as it may already be normalized.
  unsigned bbID = bbNumPass->getID(DBB.BB);
  unsigned long index = findPreviousID(DBB.BB->getParent(),
                                       DBB.index,
                                       RecordType::BBType,
                                       trace[DBB.index].tid,
                                       bbID);
  if (index == maxIndex) { // Could not find required trace entry
    DEBUG(errs() << "Buggy values found at normalization. Function name: "
                 << DBB.BB->getParent()->getName().str() << "\n");
    BuggyValues.insert(DBB.BB);
    NumStaticBuggyVal++;
    return false;
  }

  // Assert that we have found the entry.
  assert(trace[index].type == RecordType::ENType ||
         (trace[index].type == RecordType::BBType && trace[index].id == bbID));

  // Update the index within the dynamic basic block and return.
  DBB.index = index;
  return true;
}

bool TraceFile::normalize(DynValue &DV) {
#if 0
  // If we're already at the end record, assume that we're normalized. We
  // should only hit the end record when we search forwards in the trace for a
  // basic block record and didn't find one (perhaps because the program
  // terminated before the basic block finished execution).
  if (trace[DV.index].type == RecordType::ENType)
    return true;
#endif

  if (BuggyValues.find(DV.V) != BuggyValues.end()) {
    NumDynBuggyVal++;
    return false;
  }

  // Get the basic block to which this value belongs.
  BasicBlock *BB = nullptr;
  if (Instruction *I = dyn_cast<Instruction>(DV.V))
    BB = I->getParent();
  else if (Argument *Arg = dyn_cast<Argument>(DV.V))
    BB = &(Arg->getParent()->getEntryBlock());

  // If this value does not belong to a basic block, don't try to normalize
  // it.
  if (!BB)
    return true;

  // Normalize the value.
  Function *fun = BB->getParent();
  unsigned bbID = bbNumPass->getID(BB);
  unsigned long normIndex = findPreviousID(fun,
                                           DV.index,
                                           RecordType::BBType,
                                           trace[DV.index].tid,
                                           bbID);
  if (normIndex == maxIndex) { // Error, could not find required trace entry
    DEBUG(errs() << "Buggy values found at normalization. Function name: "
                 << fun->getName().str() << "\n");
    BuggyValues.insert(DV.V);
    NumStaticBuggyVal++;
    return false;
  }

  // Assert that we found the basic block for this value.
  assert(trace[normIndex].type == RecordType::BBType);

  // Modify the dynamic value's index to be the normalized index into the
  // trace.
  DV.index = normIndex;
  return true;
}

//===----------------------------------------------------------------------===//
//                         Private TraceFile Implementations
//===----------------------------------------------------------------------===//

/// This is a comparison operator that is specially designed to determine if
/// an overlapping entry exists within a set.  It doesn't implement
/// standard less-than semantics.
struct EntryCompare {
  bool operator()(const Entry &e1, const Entry &e2) const {
    return e1.address < e2.address && (e1.address + e1.length - 1 < e2.address);
  }
};

/// \brief Scan forward through the entire trace and record store instructions,
/// creating a set of memory intervals that have been written.
///
/// Along the way, determine if there are load records for which no previous
/// store record can match.  Mark these load records so that we don't try to
/// find their matching stores when peforming the dynamic backwards slice.
/// This function will be called in the constructor.
/// This algorithm should be O(n*logn) where n is the number of elements in the
/// trace.
void TraceFile::fixupLostLoads() {
  // Set of written memory locations
  set<Entry, EntryCompare> Stores;

  // Loop through the entire trace to look for lost loads.
  for (unsigned long index = 0;
       trace[index].type != RecordType::ENType;
       ++index)
    // Take action on the various record types.
    switch (trace[index].type) {
      case RecordType::STType: {
        // Add this entry to the store if it wasn't there already.  Note that
        // the entry we're adding may overlap with multiple previous stores,
        // so continue merging store intervals until there are no more.
        Entry newEntry = trace[index];
        set<Entry>::iterator st;
        while ((st = Stores.find(newEntry)) != Stores.end()) {
          // An overlapping store was performed previous.  Remove it and create
          // a new store record that encompasses this record and the existing
          // record.
          uintptr_t address = (st->address < newEntry.address) ?
                               st->address : newEntry.address;
          uintptr_t endst   =  st->address + st->length - 1;
          uintptr_t end     =  newEntry.address + newEntry.length - 1;
          uintptr_t maxend  = (endst < end) ? end : endst;
          uintptr_t length  = maxend - address + 1;
          newEntry.address = address;
          newEntry.length = length;
          Stores.erase(st);
        }

        Stores.insert(newEntry);
        break;
      }
      case RecordType::LDType: {
        // If there is no overlapping entry for the load, then it is a lost
        // load.  Change its address to zero.
        if (Stores.find(trace[index]) == Stores.end()) {
          DEBUG(dbgs() << "Fixing load for index " << index << "\n");
          trace[index].address = 0;
        }
        break;
      }
      default:
        break;
    }
}

/// Build a map from functions to their runtime trace address
///
/// FIXME: Can we record this during trace generation or similar to lsNumPass
/// This also doesn't work with indirect calls.
///
/// Description: Scan forward through the entire trace and record the
/// runtime function addresses from the trace.  and map the functions
/// in this run to their corresponding trace function addresses which
/// can possibly be different This algorithm should be n*c where n
/// is the number of elements in the trace.
void TraceFile::buildTraceFunAddrMap(void) {
  // Loop through the entire trace to look for Call records.
  for (unsigned long index = 0;
       trace[index].type != RecordType::ENType;
       ++index) {
    // Take action on the call record types.
    if (trace[index].type == RecordType::CLType) {
      Instruction *V = lsNumPass->getInstByID(trace[index].id);
      if (CallInst *CI = dyn_cast<CallInst>(V))
        // For recursion through indirect function calls, it'll be 0 and it
        // will not work
        if (Function *calledFun = CI->getCalledFunction())
          if (traceFunAddrMap.find(calledFun) == traceFunAddrMap.end())
            traceFunAddrMap[calledFun] = trace[index].address;
    }
  }

  DEBUG(dbgs() << "traceFunAddrMap.size(): " << traceFunAddrMap.size() << "\n");
}

/// Scan from the beginning to the end of the trace
/// Mark ranges for TXs
void TraceFile::initTXSegment(void) {
  unsigned long tx_begin_index = 0;
  for (unsigned long index = 0; index <= maxIndex; ++index) {
    // Get Call record
    if (trace[index].type != RecordType::CLType) {
      continue;
    }

    if (trace[index].length == 0) {
      continue;
    }

    // TODO: hard-coded for now
    // Call inst len(1) -> tx_begin
    if (trace[index].length == 1) {
      tx_begin_index = index + 3;
    // Call inst len(2) -> tx_begin
    } else if (trace[index].length == 2){
      assert(tx_begin_index != 0);
      unsigned long tx_end_index = index - 1;
      IndexRange indexRange(tx_begin_index, tx_end_index);
      txSegment.push_back(indexRange);

      tx_begin_index = 0;
    }
  }

  list <IndexRange>::iterator it;
  for (it = txSegment.begin(); it != txSegment.end(); ++it) {
    DEBUG(dbgs() << "IndexRange: start="
                 << it->getStart() << ", end=" << it->getEnd() << "\n");
  }

  totalTXs = txSegment.size();
  currTXs = totalTXs;

}

/// This method searches backwards in the trace file for an entry of the
/// specified type and ID.
///
/// \param start_index - The index in the trace file which will be examined
///                      first for a match.
/// \param type - The type of entry for which the caller searches.
/// \param id - The ID field of the entry for which the caller searches.
/// \return The index in the trace of entry with the specified type and ID is
/// returned.
unsigned long TraceFile::findPreviousID(unsigned long start_index,
                                        RecordType type,
                                        pthread_t tid,
                                        const unsigned id) {
  // Start searching from the specified index and continue until we find an
  // entry with the correct ID.
  unsigned long index = start_index;
  while (true) {
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].id == id)
      return index;
    if (index == 0)
      break;
    --index;
  }

  // We didn't find the record.  If this is a basic block record, then grab the
  // END record.
  // ************* WHY?????? Before we may end before flushing all BB ends *****
  if (type == RecordType::BBType) {
    for (index = maxIndex; trace[index].type != RecordType::ENType; --index)
      ;
    return index;
  }

  // Report fatal error that we've found the entry for which we're looking.
  report_fatal_error("Did not find desired trace entry!");
}

/// This method searches backwards in the trace file for an entry of the
/// specified type and ID taking recursion into account.
/// FIXME: Doesn't work for recursion through indirect function calls
///
/// \param fun - Function to which this search entry belongs.
///              Needed to check recursion.
/// \param start_index - The index in the trace file which will be examined
///                      first for a match.
/// \param type - The type of entry for which the caller searches.
/// \param tid - The thread ID
/// \param ids - A set of ids of the entry for which the caller searches.
/// \return The index in the trace of entry with the specified type and ID is
/// returned; If it can't find a matching entry it'll return maxIndex as error
/// code.
unsigned long TraceFile::findPreviousID(Function *fun,
                                        unsigned long start_index,
                                        RecordType type,
                                        pthread_t tid,
                                        const set<unsigned> &ids) {
  // Get the runtime trace address of this function fun
  // If this function is not called or called through indirect call we won't
  // have its runtime trace address. So, we can't track recursion for them.
  uintptr_t funAddr;
  if (traceFunAddrMap.find(fun) != traceFunAddrMap.end())
     funAddr = traceFunAddrMap[fun];
  else
     funAddr = ~0; // Make sure nothing matches in this case. @TODO Check again.

  unsigned long index = start_index;
  signed nesting = 0;
  do {
    assert(nesting >= 0);
    
    // We have found an entry matching our criteria.  If the nesting level is
    // zero, then this is our entry.  Otherwise, we know that we've found a
    // matching entry within a nested basic block entry.
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        ids.count(trace[index].id)) {
      if (nesting == 0)
        return index;
      // If we are seraching for call record, then there may be problem due to
      // self recursion. In this case, we return the index at nesting level 1
      // as we have already seen its return record and this call record should
      // have decreased the nesting level to 0.
      else if (nesting == 1 &&
               type == RecordType::CLType &&
               trace[index].type == RecordType::CLType &&
               trace[index].tid == tid &&
               trace[index].address == funAddr)
        return index;
    }

    // If this is a return entry with an address value equal to the function
    // address of the current value, we know that we've hit a recursive (i.e.,
    // nested) execution of the basic block.  Increase the nesting level.
    if (trace[index].type == RecordType::RTType &&
        trace[index].tid == tid &&
        trace[index].address == funAddr)
      ++nesting;

    // We have found an call entry with an address value equal to the function
    // address of the current value, we know that we've found a matching entry
    // within a nested basic block entry and should therefore decrease the
    // nesting level.
    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == tid &&
        trace[index].address == funAddr)
      --nesting;

    --index; // Check the next index.
  } while (index != 0);

  // @TODO: delete this
  return maxIndex;

  if (type == RecordType::BBType) {
    for (index = maxIndex; trace[index].type != RecordType::ENType; --index)
      ;
    return index;
  }

  report_fatal_error("Did not find desired trace of basic block!");
}

unsigned long TraceFile::findPreviousID(Function *fun,
                                        unsigned long start_index,
                                        RecordType type,
                                        pthread_t tid,
                                        const unsigned id) {
  set<unsigned> ids;
  ids.insert(id);
  return findPreviousID(fun, start_index, type, tid, ids);
}

/// This method is like findPreviousID() but takes recursion into account.
/// \param start_index - The index before which we should start the search
///                      (i.e., we first examine the entry in the log file
///                      at start_index - 1).
/// \param type - The type of entry for which we are looking.
/// \param id - The ID of the entry for which we are looking.
/// \param nestedID - The ID of the basic block to use to find nesting levels.
unsigned long TraceFile::findPreviousNestedID(unsigned long start_index,
                                              RecordType type,
                                              pthread_t tid,
                                              const unsigned id,
                                              const unsigned nestedID) {
  // Assert that we're starting our backwards scan on a basic block entry.
  assert(trace[start_index].type == RecordType::BBType);
  // Assert that we're not looking for a basic block index, since we can only
  // use this function when entry belongs to basic block nestedID.
  assert(type != RecordType::BBType);
  assert(start_index > 0);

  // Start searching from the specified index and continue until we find an
  // entry with the correct ID.
  // This works because entry id belongs to basicblock nestedID. So
  // any more occurance of nestedID before id means a recursion.
  unsigned long index = start_index;
  unsigned nesting = 0;
  do {
    // Check the next index.
    --index;
    // We have found an entry matching our criteria.  If the nesting level is
    // zero, then this is our entry.  Otherwise, we know that we've found a
    // matching entry within a nested basic block entry and should therefore
    // decrease the nesting level.
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].id == id) {
      if (nesting == 0) {
        return index;
      } else {
        --nesting;
        continue;
      }
    }

    // If this is a basic block entry with an idential ID to the first basic
    // block on which we started, we know that we've hit a recursive
    // (i.e., nested) execution of the basic block.
    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == tid &&
        trace[index].id == nestedID)
      ++nesting;
  } while (index != 0);

  // We've searched and didn't find our ID at the proper nesting level.
  report_fatal_error("No proper basic block at the nesting level");
}

/// This method finds the next entry in the trace file that has the specified
/// type and ID.  However, it also handles nesting.
unsigned long TraceFile::findNextNestedID(unsigned long start_index,
                                          RecordType type,
                                          const unsigned id,
                                          const unsigned nestID,
                                          pthread_t tid) {
  // This works because entry id belongs to basicblock nestedID. So any more
  // occurance of nestedID before id means a recursion.
  unsigned nesting = 0;
  unsigned long index = start_index;
  while (true) {
    // If we've searched past the end of the trace file, stop searching.
    if (index > maxIndex)
      break;

    // If we've found the entry for which we're searching, check the nesting
    // level.  If it's zero, we've found our entry.  If it's non-zero, decrease
    // the nesting level and keep looking.
    if (trace[index].type == type &&
        trace[index].id == id &&
        trace[index].tid == tid) {
      if (nesting == 0)
        return index;
      else
        --nesting;
    }

    // If we find a store/any instruction matching the nesting ID, then we've
    // left one level of recursion.
    // TODO according to the giri comments here, "store/any instruction"
    // so we need to filter out BBs
    if (trace[index].type != RecordType::BBType &&
        trace[index].id == nestID &&
        trace[index].tid == tid)
      ++nesting;

    ++index;
  }

  errs() << "start_index: " << start_index
         << " type: " << static_cast<char>(type)
         << " id: " << id
         << " nestID: " << nestID << "\n";
  report_fatal_error("Did not find desired subsequent entry in trace!");
}

/// This method searches forwards in the trace file for an entry of the
/// specified type and ID.
///
/// This method assumes that a subsequent entry in the trace *will* match the
/// type and address criteria.  Asserts in the code will ensure that this is
/// true when this code is compiled with assertions enabled.
///
/// \param start_index - The index in the trace file which will be examined first
///                      for a match.
/// \param type      - The type of entry for which the caller searches.
/// \param tid - The thread id
/// \param address   - The address of the entry for which the caller searches.
/// \return The index in the trace of entry with the specified type and
/// address is returned.
unsigned long TraceFile::findNextAddress(unsigned long start_index,
                                         RecordType type,
                                         pthread_t tid,
                                         const uintptr_t address) {
  // Start searching from the specified index and continue until we find an
  // entry with the correct type.
  unsigned long index = start_index;
  while (index <= maxIndex) {
    if (trace[index].type == type &&
        trace[index].tid == tid &&
        trace[index].address == address)
      return index;
    ++index;
  }

  errs() << "start_index: " << start_index
         << " type: " << static_cast<char>(type)
         << " tid: " << tid
         << " address: " << address << "\n";
  report_fatal_error("Did not find desired subsequent entry in trace!");
}

/// Given a dynamic value representing a phi-node, determine which basic block
/// was executed before the phi-node's basic block and add the correct dynamic
/// input to the phi-node to the backwards slice.
///
/// \param[in] DV - The dynamic phi-node value.
/// \param[out] Sources - The argument is added to this container
void TraceFile::getSourcesForPHI(DynValue &DV, Worklist_t &Sources) {
  // Get the PHI instruction.
  PHINode *PHI = dyn_cast<PHINode>(DV.V);
  assert(PHI && "Caller passed us a non-PHI value!\n");

  // Since we're lazily finding instructions in the trace, we first need to
  // find the location in the trace of the phi node.
  // @FIXME: IS IT NEEDED AFTER NORMALIZE()??
  Function *Func = PHI->getParent()->getParent();
  unsigned phiID = bbNumPass->getID(PHI->getParent());
  unsigned long block_index = findPreviousID(Func,
                                             DV.index,
                                             RecordType::BBType,
                                             trace[DV.index].tid,
                                             phiID);
  if (block_index == maxIndex) { // Could not find required trace entry
    errs() << __func__ << " failed DV.\n";
    return;
  }
  assert(block_index > 0);

  // Find a previous entry in the trace for a basic block which is a
  // predecessor of the PHI's basic block.
  set<unsigned> predIDs;
  for (unsigned index = 0; index < PHI->getNumIncomingValues(); ++index)
    predIDs.insert(bbNumPass->getID(PHI->getIncomingBlock(index)));
  unsigned long pred_index = findPreviousID(Func,
                                            block_index - 1,
                                            RecordType::BBType,
                                            trace[DV.index].tid,
                                            predIDs);
  if (pred_index == maxIndex) { // Could not find required trace entry
    errs() << __func__ << " failed BLOCK.\n";
    return;
  }

  // Get the ID of the predecessor basic block and figure out which incoming
  // PHI value should be added to the backwards slice.
  unsigned predBBID = trace[pred_index].id;
  for (unsigned index = 0; index < PHI->getNumIncomingValues(); ++index)
    if (predBBID == bbNumPass->getID(PHI->getIncomingBlock(index))) {
      Value *V = PHI->getIncomingValue(index);
      DynValue NDV = DynValue(V, pred_index);
      addToWorklist(NDV, Sources, DV);
      return;
    }

  report_fatal_error("No predecessor BB found for PHI!");
}

/// Given a dynamic use of a function's formal argument, find the dynamic
/// value that is the corresponding actual argument.
///
/// \param[in] DV - The dynamic argument value. The LLVM value must be an
///             Argument. DV is not required to be normalized.
/// \param[out] Sources - The argument is added to this container
void TraceFile::getSourcesForArg(DynValue &DV, Worklist_t &Sources) {
  // Get the argument from the dynamic instruction instance.
  Argument *Arg = dyn_cast<Argument>(DV.V);
  assert(Arg && "Caller passed a non-argument dynamic instance!\n");

  // If this is an argument to main(), then we've traced back as far as
  // possible.  Don't trace any further.
  if (Arg->getParent()->getName().str() == "main")
    return;

  // Lazily update our location within the trace file to the last execution of
  // the function's entry basic block.  We must find the proper location in
  // the trace before looking for the call instruction within the trace, and
  // we don't require that the caller normalized the dynamic value.
  if (!normalize(DV))
    return;

  // Now look for the call entry that calls this function.  The basic block
  // contains the address of the function to which the argument belongs, so we
  // just need to find a matching call entry that calls this instruction.
  assert(DV.index > 0);
  unsigned long callIndex = DV.index - 1;
  while (callIndex > 0) {
    if (trace[callIndex].type == RecordType::CLType &&
        trace[callIndex].tid == trace[DV.index].tid &&
        trace[callIndex].address == trace[DV.index].address)
      break;
    --callIndex;
  }
  assert(callIndex < DV.index);
  // FIXME
  if (trace[callIndex].type != RecordType::CLType ||
      trace[callIndex].tid != trace[DV.index].tid ||
      trace[callIndex].address != trace[DV.index].address) {
    errs() << "For some variable length functions like ap_rprintf in apache, "
              "call records missing. Stop here for now. Fix it later\n";
    DV.getValue()->dump();
    return;
  }

  // Now that we have found the call instruction within the trace, find the
  // static, LLVM call instruction that goes with it.
  unsigned callid = trace[callIndex].id;
  CallInst *CI = dyn_cast<CallInst>(lsNumPass->getInstByID(callid));
  assert(CI);

  // Scan forward through the trace to find the execution of the basic block
  // that contains the call.  If we encounter call records with the same ID,
  // then we have recursive executions of the function containing the call
  // taking place.  Keep track of the nesting so that the call instruction is
  // matched with the basic block record for its dynamic execution.
  bool found = false;
  unsigned nesting = 0;
  unsigned long index = callIndex;
  unsigned bbid = bbNumPass->getID(CI->getParent());
  while (!found) {
    // Assert that we actually find the basic block record eventually.
    assert(index <= maxIndex);

    // If we find a call record entry with the same ID as the call whose basic
    // block we're looking for, increasing the nesting level.
    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == trace[callIndex].tid &&
        trace[index].id == trace[callIndex].id) {
      ++nesting;
      ++index;
      continue;
    }

    // If we find a basic block record for the basic block in which the call
    // instruction is contained, decrease the nesting indexing.  If the
    // nesting is zero, then we've found our basic block entry.
    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == trace[callIndex].tid &&
        trace[index].id == bbid) {
      if (--nesting == 0) {
        // We have found our call instruction.  Add the actual argument in
        // the call instruction to the backwards slice.
        //DynValue newDynValue = DynValue(CI->getOperand(Arg->getArgNo()), index);
        //addToWorklist(newDynValue, Sources, DV);
        //return;
        break;
      }
    }

    // If we find an end record, then the program was terminated before the
    // basic block finished execution.  In that case, just use the index of
    // the end record; it's the best we can do.
    if (trace[index].type == RecordType::ENType) {
      // We have found our call instruction.  Add the actual argument in
      // the call instruction to the backwards slice.
      //DynValue newDynValue = DynValue(CI->getOperand(Arg->getArgNo()), index);
      //addToWorklist(newDynValue, Sources, DV);
      //return;
      break;
    }

    // Move on to the next record.
    ++index;
  }

  // Assert that we actually find the basic block record eventually.
  assert(index <= maxIndex);

  // If it is external library call like pthread_create which can call
  // a function externally, then there will be 2 call records of
  // original lib call and externally called function with the same call id.
  // In that case just include all parameters of lib call as we don't have that code
  Function *CalledFunc = CI->getCalledFunction();
  if (CalledFunc && CalledFunc->isDeclaration()) {
   // If pthread_create is called then handle it personally as it calls
   // functions externally and add an extra call for the externally called
   // functions with the same id so that returns can match with it.  In
   // addition to a function call
    if (CalledFunc->getName().str() == "pthread_create") {
      for (uint i=0; i<CI->getNumOperands()-1; i++)
        if (!isa<Constant>(CI->getOperand(index))) {
          DynValue NDV = DynValue(CI->getOperand(i), index);
          addToWorklist(NDV, Sources, DV);
        }
      return;
    }
    // @TODO We should handle other external functions conservatively by
    // adding all arguments
  } else {
    // We have found our call instruction.  Add the actual argument in
    // the call instruction to the backwards slice.
    DynValue NDV = DynValue(CI->getOperand(Arg->getArgNo()), index);
    addToWorklist(NDV, Sources, DV);
    return;
  }
}

/// Determine whether two entries access overlapping memory regions.
/// FIXME: Do we need to consider the tid?
/// \return true if the objects have some overlap memory locations else false
static inline bool overlaps(const Entry &first, const Entry &second) {
  // Case 1: The objects do not overlap and the first object is located at a
  // lower address in the address space.
  if (first.address < second.address &&
      first.address + first.length - 1 < second.address)
    return false;

  // Case 2: The objects do not overlap and the second object is located at a
  // lower address in the address space.
  if (second.address < first.address &&
      second.address + second.length - 1 < first.address)
    return false;

  // The objects must overlap in some way.
  return true;
}

/// This method, given a dynamic value that reads from memory, will find the
/// dynamic value(s) that stores into the same memory.
///
/// \param DV[in] - the dynamic value of the load instruction
/// \param Sources[out] - the work list to add the related values
/// \param store_index - the index in the trace file to start with
/// \param load_entry - the load entry
void TraceFile::findAllStoresForLoad(DynValue &DV,
                                     Worklist_t &Sources,
                                     long store_index,
                                     const Entry load_entry) {
  while (store_index >= 0) {
    if (trace[store_index].type == RecordType::STType &&
        overlaps(trace[store_index], load_entry)) {
      // Find the LLVM store instruction(s) that match this dynamic store
      // instruction.
      Instruction *SI = lsNumPass->getInstByID(trace[store_index].id);
      assert(SI);

      // Scan forward through the trace to get the basic block in which the
      // store was executed.
      unsigned storeBBID = bbNumPass->getID(SI->getParent());
      // Scan from store_index+1, skipping itself
      unsigned long bbindex = findNextNestedID(store_index + 1,
                                               RecordType::BBType,
                                               storeBBID,
                                               trace[store_index].id,
                                               trace[store_index].tid);
      // Record the store instruction as a source.
      // FIXME: This should handle *all* stores with the ID.  It is possible
      // that this occurs through function cloning.
      DynValue NDV = DynValue(SI, bbindex);
      addToWorklist(NDV, Sources, DV);

      Entry &store_entry = trace[store_index];
      // Find stores corresponding to any non-overlapping part of load
      // before the start of matched store
      if (load_entry.address < store_entry.address) {
        Entry new_entry;
        new_entry.address = load_entry.address;
        new_entry.length = store_entry.address - load_entry.address;
        findAllStoresForLoad(DV, Sources, store_index - 1, new_entry);
      }

      // Find stores corresponding to any non-overlapping part of load
      // before the start of matched store
      unsigned long store_end = store_entry.address + store_entry.length;
      unsigned long load_end = load_entry.address + load_entry.length;
      if (store_end < load_end) {
        Entry new_entry;
        new_entry.address = store_end;
        new_entry.length = load_end - store_end;
        findAllStoresForLoad(DV, Sources, store_index - 1, new_entry);
      }
      break;
    }
    --store_index;
  }

  // It is possible that this load reads data that was stored by something
  // outside of the program or that was initialzed by the load (e.g., global
  // variables).

  // If we can't find the source of the load, then just ignore it.  The trail
  // ends here.
  if (store_index == -1) {
    // This load may be uninitialized or we don't support a special function
    // which may be storing to this load
    DEBUG(dbgs() << "We can't find the source of the load:");
    DEBUG(DV.getValue()->print(dbgs()));
    Instruction *LI = dyn_cast<Instruction>(DV.getValue());
    DEBUG(dbgs() << " ID: " << lsNumPass->getID(LI));
    DEBUG(dbgs() << "\n");
    ++lostLoadsTraced;
  }
}

/// This method, given a dynamic value that reads from memory, will find the
/// dynamic value(s) that stores into the same memory.
///
/// \param[in] DV - The dynamic value which reads the memory.
/// \param[in] count - The number of loads performed by this instruction.
/// \param[out] Sources - The argument is added to this container
void TraceFile::getSourcesForLoad(DynValue &DV,
                                  Worklist_t &Sources,
                                  unsigned count) {
  // Get the load instruction from the dynamic value instance.
  Instruction *I = dyn_cast<Instruction>(DV.V);
  assert(I && "Called with non-instruction dynamic instruction instance!\n");

  // Get the ID of the instruction performing the load(s) as well as the ID of
  // the basic block in which the instrucion resides.
  unsigned loadID = lsNumPass->getID(I);
  assert(loadID && "load does not have an ID!\n");
  unsigned bbID = bbNumPass->getID(I->getParent());

  // Thre are no loads to find sources for possible for sprintf with only
  // scalar variables
  if (count == 0)
     return;

  if (!normalize(DV))
    return;

  // Search back in the log to find the first load entry that both belongs to
  // the basic block of the load.  Remember that we must handle nested basic
  // block execution when doing this.
  std::vector<unsigned long> load_indices(count);
  unsigned long start_index = findPreviousNestedID(DV.index,
                                                   RecordType::LDType,
                                                   trace[DV.index].tid,
                                                   loadID,
                                                   bbID);
  load_indices[0]= start_index;
  // If there are more load records to find, search back through the log to
  // find the most recently executed load with the same ID as this load.  Note
  // that these should be immediently before the load record; therefore, we
  // should not need to worry about nesting.
  for (unsigned index = 1; index < count; ++index) {
    start_index = findPreviousID(start_index - 1,
                                 RecordType::LDType,
                                 trace[DV.index].tid,
                                 loadID);
    load_indices[index]= start_index;
  }

  // For each load, trace it back to the instruction which stored to an
  // overlapping memory location.
  for (unsigned index = 0; index < count; ++index) {
    // Scan back through the trace to find the most recent store(s) that
    // stored to these locations.
    ++totalLoadsTraced;
    long block_index = load_indices[index];

    // Don't bother performing the scan if the address is zero.  This means
    // that it's a lost load for which no matching store exists.
    if (!trace[block_index].address) {
      ++lostLoadsTraced;
      continue;
    }

    long store_index = block_index - 1;
    findAllStoresForLoad(DV, Sources, store_index, trace[block_index]);

    /*
    while ((store_index >= 0) &&
           ((trace[store_index].type != RecordType::STType) ||
            (!overlaps (trace[store_index], trace[block_index])))) {
      --store_index;
#if 0
      DEBUG(printf("block_index = %ld, store_index = %ld\n", block_index, store_index));
      fflush(stdout);
#endif
    }
#if 0
    DEBUG(printf("exited store_index = %ld\n", store_index));
    fflush(stdout);
#endif

    //
    // It is possible that this load reads data that was stored by something
    // outside of the program or that was initialzed by the load (e.g., global
    // variables).
    //
    // If we can't find the source of the load, then just ignore it.  The trail
    // ends here.
    //
    if (store_index == -1) {
#if 0
      cerr << "Load: " << hex << trace[block_index].address << "\n";
      cerr << "We can't find the source of the load. This load may be "
                << "uninitialized or we don't support a special function "
                << "which may be storing to this load." << endl;
#endif
      ++lostLoadsTraced;
      continue;
    }

    assert(shouldBeLost == false);
    assert(trace[store_index].type    == RecordType::STType);
    assert(overlaps(trace[store_index], trace[block_index]));

    //
    // Find the LLVM store instruction(s) that match this dynamic store
    // instruction.
    //
    Value *V = lsNumPass->getInstByID(trace[store_index].id);
    assert(V);
    Instruction *SI = dyn_cast<Instruction>(V);
    assert(SI);

    //
    // Scan forward through the trace to get the basic block in which the store
    // was executed.
    //

    // ****** I think, we are unnecessarily going forward to the basic block end and then
    // coming back to find the invariant failurs. We can optimize it avoid dual traversal
    // We'll have to change to point to directly load indices counting the nesting levels ******

    unsigned storeID = lsNumPass->getID(SI);
    unsigned storeBBID = bbNumPass->getID(SI->getParent());
    unsigned long bbindex = findNextNestedID(store_index,
                                             RecordType::BBType,
                                             storeBBID,
                                             storeID);

    //
    // Record the store instruction as a source.
    //
    // FIXME:
    //  This should handle *all* stores with the ID.  It is possible that this
    //  occurs through function cloning.
    //
    DynValue newDynValue =  DynValue(V, bbindex);
    addToWorklist(newDynValue, Sources, DV);
    */
  }

  return;
}

/// Determine if the dynamic value is a call to a specially handled function
/// and, if so, find the sources feeding information into that dynamic
/// function.
///
/// \return true  - This is a call to a special function.
/// \return false - This is not a call to a special function.
bool TraceFile::getSourcesForSpecialCall(DynValue &DV,
                                         Worklist_t &Sources) {
  // Get the call instruction of the dynamic value.  If it's not a call
  // instruction, then it obviously isn't a call to a special function.
  Instruction *I = dyn_cast<Instruction>(DV.V);
  if (!(isa<CallInst>(I) || isa<InvokeInst>(I)))
    return false;

  // If this is a debug intrinsic, do not backtrack its inputs.  It is just a
  // place-holder for debugging information.
  if (isa<DbgInfoIntrinsic>(I))
    return true;

  // Get the called function.  If we cannot determine the called function,
  // then this is not a special function call (we do not support indirect
  // calls to special functions right now).
  CallSite CS(I);
  Function *CalledFunc = CS.getCalledFunction();
  if (!CalledFunc)
    return false;

  // Get the void pointer type since we may need it.
  LLVMContext &Context = I->getParent()->getParent()->getParent()->getContext();
  Type *Int8Type = IntegerType::getInt8Ty(Context);
  const Type *VoidPtrType = PointerType::getUnqual(Int8Type);

  // Get the current index into the dynamic trace; we'll need that as well.
  unsigned trace_index = DV.index;

  // Handle call instruction to a special function specially
  const StringRef name = CalledFunc->getName();
  if (name.startswith("llvm.memset.") || name == "calloc") {
    // Add all arguments (including pointer values) into the backwards
    // dynamic slice. Not including called function pointer now.
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    // We don't read from any memory buffer, so return true and be done.
    return true;
  } else if (name.startswith("llvm.memcpy.") ||
             name.startswith("llvm.memmove.") ||
             name == "strcpy" ||
             name == "strlen") {
    // Scan through the arguments to the call.  Add all the values to the set
    // of sources. For the destination pointer, backtrack to find the storing
    // instruction. Not including called function pointer now.
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    // Find the stores that generate the values that we load.
    getSourcesForLoad(DV, Sources);
    return true;
  } else if (name == "strcat") {
    // Scan through the arguments to the call.  Add all the values to the set
    // of sources. For the destination pointer, backtrack to find the storing
    // instruction. Not including called function pointer now.
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    // Find the stores that generate the values that we load twice.
    getSourcesForLoad(DV, Sources, 2);
    return true;
  } else if (name == "tolower" || name == "toupper") {
    // Not needed as there are no loads and stores
  } else if (name == "fscanf") {
    //scanf/fscanf not so important and printf, fprintf not needed
    // TODO
  } else if (name == "sscanf") {
    // TODO
  } else if (name == "sprintf") {
    // Scan through the arguments to the call.  Add all the values
    // to the set of sources.  If they are pointers to character
    // arrays, backtrack to find the storing instruction.
    // Not including called function pointer now.
    unsigned numCharArrays = 0;
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        // All scalars(including the pointers) into the dynamic backwards slice.
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
        // If it's a character ptr but not the destination ptr or format string
        if (CS.getArgument(index)->getType() == VoidPtrType && index >= 2)
          ++numCharArrays;
      }
    // Find the stores that generate the values that we load.
    getSourcesForLoad(DV, Sources, numCharArrays);
    return true;
  } else if (name == "fgets") {
    // Add all arguments (including pointer values) into the backwards
    // dynamic slice. Not including called function pointer now.
    for (unsigned index = 0; index < CS.arg_size(); ++index)
      if (!isa<Constant>(CS.getArgument(index))) {
        DynValue NDV = DynValue(CS.getArgument(index), trace_index);
        addToWorklist(NDV, Sources, DV);
      }
    // We don't read from any memory buffer, so return true and be done.
    return true;
  }

  // This is not a call to a special function.
  return false;
}

/// Given a call instruction, this method searches backwards in the trace file
/// to match the return inst with its coressponding call instruction
///
/// \param start_index - The index in the trace file which will be examined
///                      first for a match. This is points to the basic block
///                      entry containing the function call in trace. Start
///                      search from the previous of start_index.
/// \param bbID - The basic block ID of the basic block containing the call
///               instruction
/// \param callID - ID of the function call instruction we are trying to match
/// \param tid - the thread id
/// \return The index in the trace of entry with the specified type and ID is
/// returned; If no such entry is found, then the end entry is returned.
unsigned long TraceFile::matchReturnWithCall(unsigned long start_index,
                                             const unsigned bbID,
                                             const unsigned callID,
                                             pthread_t tid) {
  // Assert that we're starting our backwards scan on a basic block entry.
  assert(trace[start_index].type == RecordType::BBType);
  assert(start_index > 0);

  // Start searching from the specified index and continue until we find an
  // entry with the correct ID. This works because entry callID belongs to
  // basicblock bbID. So any more occurance of bbID before callID means a
  // recursion.
  unsigned long index = start_index;
  unsigned nesting = 0;
  do {
    // Check the next index.
    --index;

    // We have found an entry matching our criteria.  If the nesting level is
    // zero, then this is our entry.  Otherwise, we know that we've found a
    // matching entry within a nested basic block entry.
    if (trace[index].type == RecordType::RTType &&
        trace[index].id == callID &&
        trace[index].tid == tid)
      if (nesting == 0)
        return index;

    // We have found an call entry with same id.  If the nesting level
    // is zero, then we didn't find a matching return. Otherwise, we
    // know that we've found a matching entry within a nested basic
    // block entry and should therefore decrease the nesting level.
    if (trace[index].type == RecordType::CLType &&
        trace[index].tid == tid &&
        trace[index].id == callID) {
      if (nesting == 0)
        report_fatal_error("Could NOT find a matching return entry for call!");
      else {
        --nesting;
        continue;
      }
    }

    // If this is a basic block entry with an idential ID to the first basic
    // block on which we started, we know that we've hit a recursive (i.e.,
    // nested) execution of the basic block.  Increase the nesting level.
    if (trace[index].type == RecordType::BBType &&
        trace[index].tid == tid &&
        trace[index].id == bbID)
      ++nesting;
  } while (index != 0);

  report_fatal_error("Can't find matching call at the proper nesting level.");
}

void TraceFile::getSourcesForCall(DynValue &DV, Worklist_t &Sources) {
  // Get the Call instruction.
  CallInst *CI = dyn_cast<CallInst>(DV.V);
  assert(CI && "Caller passed us a non-call value!\n");

  // Since we're lazily finding instructions in the trace, we first need to
  // find the location in the trace of the call.
  // FIXME: Are all these calls duplicate since we call it after removing from
  // worklist?
  if (!normalize(DV))
    return;

  Function *CalledFunc = CI->getCalledFunction();
  if (!CalledFunc) {
    // If it doesn't have a CalledFunc (like asm call), the previous code use
    // the function containing this call instruction to get the index.
    // This is Wrong!
    // It will violate the assert(nesting >= 0) in findPreviousID function.
    // TODO: we need to think how to get dependence of a call instruction
    return;
    // This is an indirect function call.  Look for its call record in the
    // trace and see what function it called at run-time.
    unsigned callID = lsNumPass->getID(CI);
    // FIXME: Will indirect call create a problem in
    // findPreviousID as indirect calls are not handled??
    Function *Func = CI->getParent()->getParent();
    unsigned long callIndex = findPreviousID(Func,
                                             DV.index,
                                             RecordType::CLType,
                                             trace[DV.index].tid,
                                             callID);
    if (callIndex == maxIndex) { // Could not find required trace entry
      errs() << __func__ << " failed to find\n";
      return;
    }

    uintptr_t fp = trace[callIndex].address;
    if (trace[callIndex + 1].type == RecordType::RTType &&
        trace[callIndex + 1].tid == trace[callIndex].tid &&
        trace[callIndex + 1].id == trace[callIndex].id &&
        trace[callIndex + 1].address == trace[callIndex].address) {
      errs() << "Most likely an (indirect) external call. Check to make sure\n";
      // Possible call to external function, just add its operands to slice
      // conservatively.
      for (unsigned index = 0; index < CI->getNumOperands(); ++index)
        if (!isa<Constant>(CI->getOperand(index))) {
          DynValue NDV = DynValue(CI->getOperand(index), DV.index);
          addToWorklist(NDV, Sources, DV);
        }
      return;
    }

    // Look for the exectuion of the basic block for the target function.
    // FIXME!!!! Do we need to take into account recursion here?? Probably NO
    unsigned long targetEntryBB = findNextAddress(callIndex + 1,
                                                  RecordType::BBType,
                                                  trace[callIndex].tid,
                                                  fp);
    if (targetEntryBB == maxIndex)
      return;

    // Get the LLVM basic block associated with the entry and, from that, get
    // the called function.
    BasicBlock *TargetEntryBB = bbNumPass->getBlock(trace[targetEntryBB].id);
    CalledFunc = TargetEntryBB->getParent();
  }
  assert(CalledFunc && "Could not find call function!\n");

  // If this is a call to an external library function, then just add its
  // operands to the slice conservatively.
  if (CalledFunc->isDeclaration()) {
    for (unsigned index = 0; index < CI->getNumOperands(); ++index)
      if (!isa<Constant>(CI->getOperand(index))) {
        DynValue NDV = DynValue(CI->getOperand(index), DV.index);
        addToWorklist(NDV, Sources, DV);
      }
    return;
  }

  // Search backwards in the trace until we find one of the basic blocks that
  // could have caused the return instruction.
  // Take into account recursion and successive calls of same function using
  // return ids of function calls in the last basic block.
  unsigned bbID = bbNumPass->getID(CI->getParent());
  unsigned callID = lsNumPass->getID(CI);
  unsigned long retindex = matchReturnWithCall(DV.index,
                                               bbID,
                                               callID, 
                                               trace[DV.index].tid);

  // FIXME FOR MUTIPLE THREADS.
  // If there are multiple threads, previous entry may not be the BB of the
  // returned function, in that case this assert may fail. May need to search
  // the last such BB entry of the trace of corresponding trace.
  unsigned long tempretindex = retindex - 1;
  while (trace[tempretindex].type != RecordType::BBType)
    tempretindex--;

  // FIXME: why records are not generated inside some calls as in stat,my_stat
  // of mysql????
  if (!(trace[tempretindex].type == RecordType::BBType &&
        trace[tempretindex].tid == trace[retindex].tid &&
        trace[tempretindex].address == trace[retindex].address)) {
    errs() << "Return and BB record doesn't match! May be due to some reason "
              "the records of a called function are not recorded as in stat "
              "function of mysql.\n";
    // Treat it as external library call in this case and add all operands
    for (unsigned index = 0; index < CI->getNumOperands(); ++index)
      if (!isa<Constant>(CI->getOperand(index))) {
        DynValue NDV = DynValue(CI->getOperand(index), DV.index);
        addToWorklist(NDV, Sources, DV);
      }
    return;
  }

  // Make the return instruction for that basic block the source of the call
  // instruction's return value.
  for (auto BB = CalledFunc->begin(); BB != CalledFunc->end(); ++BB) {
    if (isa<ReturnInst>(BB->getTerminator()))
      if (bbNumPass->getID(&*BB) == trace[tempretindex].id) {
        DynValue NDV = DynValue(BB->getTerminator(), tempretindex);
        addToWorklist(NDV, Sources, DV);
      }
  }

  /* // For return matching using BB record length
  for (Function::iterator BB = CalledFunc->begin();  BB != CalledFunc->end(); ++BB) {
    if (isa<ReturnInst>(BB->getTerminator())) {
      if (bbNumPass->getID(BB) == trace[retindex].id) {
          DynValue newDynValue = DynValue(BB->getTerminator(), retindex);
          addToWorklist(newDynValue, Sources, DV);
      }
    }
  }
  */

  /*
  //
  // Now find all return instructions that could have caused execution to
  // return to the caller (i.e., to this call instruction).
  //
  map<unsigned, BasicBlock *> retMap;
  set<unsigned> retIDs;
  for (Function::iterator BB = CalledFunc->begin();
       BB != CalledFunc->end();
       ++BB) {
    if (isa<ReturnInst>(BB->getTerminator())) {
      retIDs.insert(bbNumPass->getID(BB));
      retMap[bbNumPass->getID(BB)] = BB;
    }
  }

  //
  // Search backwards in the trace until we find one of the basic blocks that
  // could have caused the return instruction.
  // Take into account recursion and successive calls of same function using
  // return ids of function calls in the last basic block.
  //

  unsigned long retindex = findPreviousID(block_index, RecordType::BBType, retIDs);


  //
  // Make the return instruction for that basic block the source of the call
  // instruction's return value.
  //
  unsigned retid = trace[retindex].id;
  DynValue newDynValue = DynValue(retMap[retid]->getTerminator(), retindex);
  addToWorklist(newDynValue, Sources, DV);
  */
}

/// Examine the trace file to determine which input of a select instruction
/// was used during dynamic execution.
void TraceFile::getSourceForSelect(DynValue &DV, Worklist_t &Sources) {
  // Get the select instruction.
  SelectInst *SI = dyn_cast<SelectInst>(DV.V);
  assert(SI && "getSourceForSelect used on non-select instruction!\n");

  // Normalize the dynamic instruction so that we know its precise location
  // within the trace file.
  if (!normalize(DV))
    return;

  // Now find the index of the most previous select instruction.
  unsigned selectID = lsNumPass->getID(SI);
  Function *Func = SI->getParent()->getParent();
  unsigned long selectIndex = findPreviousID(Func,
                                             DV.index,
                                             RecordType::PDType,
                                             trace[DV.index].tid,
                                             selectID);
  if (selectIndex == maxIndex) { // Could not find required trace entry
    errs() << __func__ << " failed to find.\n";
    return;
  }

  // Assert that we've found the trace record we want.
  assert(trace[selectIndex].type == RecordType::PDType);
  assert(trace[selectIndex].id == selectID);

  // Determine which argument was used at run-time based on the trace.
  unsigned predicate = trace[selectIndex].address;
  Value *Operand = predicate ? SI->getTrueValue() : SI->getFalseValue();
  DynValue NDV = DynValue(Operand, DV.index);
  addToWorklist(NDV, Sources, DV);

  return;
}
