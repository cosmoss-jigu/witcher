//===- TracingNoGiri.cpp - Dynamic Slicing Trace Instrumentation Pass -----===//
//
//                          Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This files defines passes that are used for dynamic slicing.
//
// TODO:
// Technically, we should support the tracing of signal handlers.  This can
// interrupt the execution of a basic block.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giri"

#include "Giri/Giri.h"
#include "Utility/Utils.h"
#include "Utility/VectorExtras.h"
#include "Utility/Debug.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <cxxabi.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

using namespace giri;
using namespace llvm;

//===----------------------------------------------------------------------===//
//                     Command Line Arguments
//===----------------------------------------------------------------------===//
// this shared command line option was defined in the Utility so
extern llvm::cl::opt<std::string> TraceFilename;

// the file contains all the ext tracing function names
cl::opt<std::string> ExtTracingFuncFilename("ext-tracing-func-file",
                                          cl::desc("External Tracing Function"),
                                          cl::init("-"));
//===----------------------------------------------------------------------===//
//                        Pass Statistics
//===----------------------------------------------------------------------===//

STATISTIC(NumBBs, "Number of basic blocks");
STATISTIC(NumPHIBBs, "Number of basic blocks with phi nodes");
STATISTIC(NumLoads, "Number of load instructions processed");
STATISTIC(NumStores, "Number of store instructions processed");
STATISTIC(NumSelects, "Number of select instructions processed");
STATISTIC(NumLoadStrings, "Number of load instructions processed");
STATISTIC(NumStoreStrings, "Number of store instructions processed");
STATISTIC(NumCalls, "Number of call instructions processed");
STATISTIC(NumExtFuns, "Number of special external calls processed, e.g. memcpy");
STATISTIC(NumFlushes, "Number of cacheline flushes instructions processed");
STATISTIC(NumFences, "Number of mfences instructions processed");

//===----------------------------------------------------------------------===//
//                        TracingNoGiri Implementations
//===----------------------------------------------------------------------===//

char TracingNoGiri::ID = 0;

static RegisterPass<TracingNoGiri>
X("trace-giri", "Instrument code to trace basic block execution");


/// This method determines whether the given basic block contains any PHI
/// instructions.
///
/// \param  BB - A reference to the Basic Block to analyze.  It is not modified.
/// \return true  if the basic block has one or more PHI instructions,
/// otherwise false.
static bool hasPHI(const BasicBlock & BB) {
  for (BasicBlock::const_iterator I = BB.begin(); I != BB.end(); ++I)
    if (isa<PHINode>(I)) return true;
  return false;
}

bool TracingNoGiri::doInitialization(Module & M) {
  // Get references to the different types that we'll need.
  Int8Type  = IntegerType::getInt8Ty(M.getContext());
  Int32Type = IntegerType::getInt32Ty(M.getContext());
  Int64Type = IntegerType::getInt64Ty(M.getContext());
  VoidPtrType = PointerType::getUnqual(Int8Type);
  VoidType = Type::getVoidTy(M.getContext());

  // init the PMEMoidType struct {i64, i64}
  std::vector<Type*> type_vector;
  type_vector.push_back(Int64Type);
  type_vector.push_back(Int64Type);
  PMEMoidType = StructType::get(M.getContext(), type_vector);

  // Get a reference to enable the recording
  EnableRecording = M.getOrInsertFunction("enableRecording",
                                         VoidType);

  // Get a reference to the run-time's initialization function
  Init = M.getOrInsertFunction("recordInit",
                               VoidType,
                               VoidPtrType);

  // Load/Store unlock mechnism
  RecordLock = M.getOrInsertFunction("recordLock",
                                     VoidType,
                                     VoidPtrType);

  // Load/Store lock mechnism
  RecordUnlock = M.getOrInsertFunction("recordUnlock",
                                       VoidType,
                                       VoidPtrType);

  // Add the function for recording the execution of a basic block.
  RecordBB = M.getOrInsertFunction("recordBB",
                                   VoidType,
                                   Int32Type,
                                   VoidPtrType,
                                   Int32Type);

  // Add the function for recording the start of execution of a basic block.
  RecordStartBB = M.getOrInsertFunction("recordStartBB",
                                        VoidType,
                                        Int32Type,
                                        VoidPtrType);

  // Add the functions for recording the execution of loads, stores, and calls.
  RecordLoad = M.getOrInsertFunction("recordLoad",
                                     VoidType,
                                     Int32Type,
                                     VoidPtrType,
                                     Int64Type);

  RecordStrLoad = M.getOrInsertFunction("recordStrLoad",
                                        VoidType,
                                        Int32Type,
                                        VoidPtrType);

  RecordStore = M.getOrInsertFunction("recordStore",
                                      VoidType,
                                      Int32Type,
                                      VoidPtrType,
                                      Int64Type);

  RecordStrStore = M.getOrInsertFunction("recordStrStore",
                                         VoidType,
                                         Int32Type,
                                         VoidPtrType);

  RecordStrcatStore = M.getOrInsertFunction("recordStrcatStore",
                                            VoidType,
                                            Int32Type,
                                            VoidPtrType,
                                            VoidPtrType);

  RecordCall = M.getOrInsertFunction("recordCall",
                                     VoidType,
                                     Int32Type,
                                     VoidPtrType,
                                     Int32Type);

  RecordExtCall = M.getOrInsertFunction("recordExtCall",
                                        VoidType,
                                        Int32Type,
                                        VoidPtrType,
                                        Int32Type);

  RecordReturn = M.getOrInsertFunction("recordReturn",
                                       VoidType,
                                       Int32Type,
                                       VoidPtrType,
                                       Int32Type);

  RecordExtCallRet = M.getOrInsertFunction("recordExtCallRet",
                                           VoidType,
                                           Int32Type,
                                           VoidPtrType);

  RecordSelect = M.getOrInsertFunction("recordSelect",
                                       VoidType,
                                       Int32Type,
                                       Int8Type);

  RecordFlush = M.getOrInsertFunction("recordFlush",
                                      VoidType,
                                      Int32Type,
                                      VoidPtrType);

  RecordFlushWrapper = M.getOrInsertFunction("recordFlushWrapper",
                                             VoidType,
                                             Int32Type,
                                             VoidPtrType,
                                             Int64Type);

  RecordFence = M.getOrInsertFunction("recordFence",
                                      VoidType,
                                      Int32Type);

  RecordMmap = M.getOrInsertFunction("recordMmap",
                                     VoidType,
                                     Int32Type,
                                     VoidPtrType,
                                     Int64Type);

  RecordTxAdd = M.getOrInsertFunction("recordTxAdd",
                                      VoidType,
                                      Int32Type,
                                      Int64Type,
                                      Int64Type,
                                      Int64Type,
                                      Int64Type);

  RecordTxAddDirect = M.getOrInsertFunction("recordTxAddDirect",
                                            VoidType,
                                            Int32Type,
                                            VoidPtrType,
                                            Int64Type);
  RecordTxAlloc = M.getOrInsertFunction("recordTxAlloc",
                                         VoidType,
                                         Int32Type,
                                         PMEMoidType,
                                         Int64Type);
  createCtor(M);
  parseExtTracingFunc();
  return true;
}

void TracingNoGiri::createCtor(Module &M) {
  // Create the ctor function.
  Type *VoidTy = Type::getVoidTy(M.getContext());
  Function *RuntimeCtor = cast<Function>
                        (M.getOrInsertFunction("giriCtor", VoidTy).getCallee());
  assert(RuntimeCtor && "Somehow created a non-function function!\n");

  // Make the ctor function internal and non-throwing.
  RuntimeCtor->setDoesNotThrow();
  RuntimeCtor->setLinkage(GlobalValue::InternalLinkage);

  // Add a call in the new constructor function to the Giri initialization
  // function.
  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry", RuntimeCtor);
  Constant *Name = stringToGV(TraceFilename, &M);
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  CallInst::Create(Init, Name, "", BB);

  // Add a return instruction at the end of the basic block.
  ReturnInst::Create(M.getContext(), BB);

  appendToGlobalCtors(M, RuntimeCtor, 65535);
}

void TracingNoGiri::parseExtTracingFunc() {
  assert((ExtTracingFuncFilename != "-") &&
         "Cannot open ext tracing func file!\n");

  // read the file line by line and add the function names into the vector
  std::ifstream file(ExtTracingFuncFilename);
  std::string str;
  while (std::getline(file, str)) {
    ext_tracing_func_vector.push_back(str);
  }
}

void TracingNoGiri::instrumentLock(Instruction *I) {
  std::string s;
  raw_string_ostream rso(s);
  I->print(rso);
  Constant *Name = stringToGV(rso.str(),
                              I->getParent()->getParent()->getParent());
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  CallInst::Create(RecordLock, Name)->insertBefore(I);
}

void TracingNoGiri::instrumentUnlock(Instruction *I) {
  std::string s;
  raw_string_ostream rso(s);
  I->print(rso);
  Constant *Name = stringToGV(rso.str(),
                              I->getParent()->getParent()->getParent());
  Name = ConstantExpr::getZExtOrBitCast(Name, VoidPtrType);
  CallInst::Create(RecordUnlock, Name)->insertAfter(I);
}

void TracingNoGiri::instrumentBasicBlock(BasicBlock &BB) {
  // Ignore the Giri Constructor function where the it is not set up yet
  if (BB.getParent()->getName() == "giriCtor")
    return;

  // Lookup the ID of this basic block and create an LLVM value for it.
  unsigned id = bbNumPass->getID(&BB);
  assert(id && "Basic block does not have an ID!\n");
  Value *BBID = ConstantInt::get(Int32Type, id);

  // Get a pointer to the function in which the basic block belongs.
  Value *FP = castTo(BB.getParent(), VoidPtrType, "", BB.getTerminator());

  Value *LastBB;
  if (isa<ReturnInst>(BB.getTerminator()))
     LastBB = ConstantInt::get(Int32Type, 1);
  else
     LastBB = ConstantInt::get(Int32Type, 0);

  // Insert code at the end of the basic block to record that it was executed.
  std::vector<Value *> args = make_vector<Value *>(BBID, FP, LastBB, 0);
  instrumentLock(BB.getTerminator());
  Instruction *RBB = CallInst::Create(RecordBB, args, "", BB.getTerminator());
  instrumentUnlock(RBB);

  // Insert code at the beginning of the basic block to record that it started
  // execution.
  args = make_vector<Value *>(BBID, FP, 0);
  Instruction *F = &*(BB.getFirstInsertionPt());
  Instruction *S = CallInst::Create(RecordStartBB, args, "", F);
  instrumentLock(S);
  instrumentUnlock(S);
}

void TracingNoGiri::instrumentMainEntryBB(BasicBlock &BB) {
  Function* fn = BB.getParent();
  if (fn->getName() != "main") {
    return;
  }
  if (&fn->getEntryBlock() != &BB) {
    return;
  }

  // Insert eh Enable Recording function at the beginning of the main function
  // entry basic block
  Instruction *F = &*(BB.getFirstInsertionPt());
  CallInst::Create(EnableRecording, "", F);
}

void TracingNoGiri::visitLoadInst(LoadInst &LI) {
  instrumentLock(&LI);

  // Get the ID of the load instruction.
  Value *LoadID = ConstantInt::get(Int32Type, lsNumPass->getID(&LI));
  // Cast the pointer into a void pointer type.
  Value *Pointer = LI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &LI);
  // Get the size of the loaded data.
  uint64_t size = TD->getTypeStoreSize(LI.getType());
  Value *LoadSize = ConstantInt::get(Int64Type, size);
  // Create the call to the run-time to record the load instruction.
  std::vector<Value *> args=make_vector<Value *>(LoadID, Pointer, LoadSize, 0);
  CallInst::Create(RecordLoad, args, "", &LI);

  instrumentUnlock(&LI);
  ++NumLoads; // Update statistics
}

void TracingNoGiri::visitSelectInst(SelectInst &SI) {
  if (SI.getCondition()->getType()->isVectorTy()) {
    return;
  }

  instrumentLock(&SI);

  // Cast the predicate (boolean) value into an 8-bit value.
  Value *Predicate = SI.getCondition();
  Predicate = castTo(Predicate, Int8Type, Predicate->getName(), &SI);
  // Get the ID of the load instruction.
  Value *SelectID = ConstantInt::get(Int32Type, lsNumPass->getID(&SI));
  // Create the call to the run-time to record the load instruction.
  std::vector<Value *> args=make_vector<Value *>(SelectID, Predicate, 0);
  CallInst::Create(RecordSelect, args, "", &SI);

  instrumentUnlock(&SI);
  ++NumSelects; // Update statistics
}

void TracingNoGiri::visitStoreInst(StoreInst &SI) {
  instrumentLock(&SI);

  // Cast the pointer into a void pointer type.
  Value * Pointer = SI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &SI);
  // Get the size of the stored data.
  uint64_t size = TD->getTypeStoreSize(SI.getOperand(0)->getType());
  Value *StoreSize = ConstantInt::get(Int64Type, size);
  // Get the ID of the store instruction.
  Value *StoreID = ConstantInt::get(Int32Type, lsNumPass->getID(&SI));
  // Create the call to the run-time to record the store instruction.
  std::vector<Value *> args=make_vector<Value *>(StoreID, Pointer, StoreSize, 0);
  CallInst *recStore = CallInst::Create(RecordStore, args, "", &SI);

  instrumentUnlock(&SI);
  // Insert RecordStore after the instruction so that we can get the value
  SI.moveBefore(recStore);
  ++NumStores; // Update statistics
}

void TracingNoGiri::visitAtomicRMWInst(AtomicRMWInst &AI) {
  instrumentLock(&AI);

  // Cast the pointer into a void pointer type.
  Value * Pointer = AI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &AI);
  // Get the size of the stored data.
  uint64_t size = TD->getTypeStoreSize(AI.getValOperand()->getType());
  Value *StoreSize = ConstantInt::get(Int64Type, size);
  // Get the ID of the store instruction.
  Value *StoreID = ConstantInt::get(Int32Type, lsNumPass->getID(&AI));
  // Create the call to the run-time to record the store instruction.
  std::vector<Value *> args=make_vector<Value *>(StoreID, Pointer, StoreSize, 0);
  CallInst *recStore = CallInst::Create(RecordStore, args, "", &AI);

  instrumentUnlock(&AI);
  // Insert RecordStore after the instruction so that we can get the value
  AI.moveBefore(recStore);
  ++NumStores; // Update statistics
}

void TracingNoGiri::visitAtomicCmpXchgInst(AtomicCmpXchgInst &AI) {
  instrumentLock(&AI);

  // Cast the pointer into a void pointer type.
  Value * Pointer = AI.getPointerOperand();
  Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &AI);
  // Get the size of the stored data.
  uint64_t size = TD->getTypeStoreSize(AI.getNewValOperand()->getType());
  Value *StoreSize = ConstantInt::get(Int64Type, size);
  // Get the ID of the store instruction.
  Value *StoreID = ConstantInt::get(Int32Type, lsNumPass->getID(&AI));
  // Create the call to the run-time to record the store instruction.
  std::vector<Value *> args=make_vector<Value *>(StoreID, Pointer, StoreSize, 0);
  CallInst *recStore = CallInst::Create(RecordStore, args, "", &AI);

  instrumentUnlock(&AI);
  // Insert RecordStore after the instruction so that we can get the value
  AI.moveBefore(recStore);
  ++NumStores; // Update statistics
}

bool TracingNoGiri::visitPmemCall(CallInst &CI, std::string name) {
  if (name == "pmem_flush") {
    // Instrument the code and add RecordFLush
    instrumentLock(&CI);
    // Cast the pointer into a void pointer type.
    Value * Pointer = CI.getArgOperand(0);
    Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &CI);
    // Get the number of bytes that will be flushed.
    Value *NumElts = CI.getOperand(1);
    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> args=make_vector<Value *>(CallID, Pointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);
    instrumentUnlock(&CI);

    CI.moveBefore(recFlush);

    ++NumFlushes;
    return true;
  }

  if (name == "pmem_drain") {
    // Instrument the code and add RecordFence
    instrumentLock(&CI);
    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> args = make_vector<Value *>(CallID, 0);
    CallInst* recFence = CallInst::Create(RecordFence, args, "", &CI);
    instrumentUnlock(&CI);

    CI.moveBefore(recFence);

    ++NumFences;
    return true;
  }

  if (name == "pmem_persist" || name == "pmem_msync" ) {
    // Instrument the code and add RecordFLush
    instrumentLock(&CI);
    // Cast the pointer into a void pointer type.
    Value * Pointer = CI.getArgOperand(0);
    Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &CI);
    // Get the number of bytes that will be flushed.
    Value *NumElts = CI.getOperand(1);
    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> args=make_vector<Value *>(CallID, Pointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);
    args = make_vector<Value *>(CallID, 0);
    CallInst* recFence = CallInst::Create(RecordFence, args, "", &CI);

    instrumentUnlock(&CI);

    CI.moveBefore(recFlush);

    ++NumFlushes;
    ++NumFences;
    return true;

  }

  if (name == "pmem_memmove_nodrain" || name == "pmem_memcpy_nodrain") {
    instrumentLock(&CI);

    // Get two pointers
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // get the num elements to be transfered
    Value *NumElts = CI.getOperand(2);

    // record load
    std::vector<Value *> args = make_vector(CallID, srcPointer, NumElts, 0);
    CallInst *recLoad = CallInst::Create(RecordLoad, args, "", &CI);

    // record store
    args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    // record flush
    args = make_vector<Value *>(CallID, dstPointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumLoads;
    ++NumStores;
    ++NumFlushes;
    return true;
  }

  if (name == "pmem_memmove_persist" || name == "pmem_memcpy_persist") {
    instrumentLock(&CI);

    // Get two pointers
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // get the num elements to be transfered
    Value *NumElts = CI.getOperand(2);

    // record load
    std::vector<Value *> args = make_vector(CallID, srcPointer, NumElts, 0);
    CallInst *recLoad = CallInst::Create(RecordLoad, args, "", &CI);

    // record store
    args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    // record flush
    args = make_vector<Value *>(CallID, dstPointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);

    // record fence
    args = make_vector<Value *>(CallID, 0);
    CallInst* recFence = CallInst::Create(RecordFence, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumLoads;
    ++NumStores;
    ++NumFlushes;
    ++NumFences;
    return true;
  }

  if (name == "pmem_memmove" || name == "pmem_memcpy") {
    instrumentLock(&CI);

    // Get two pointers
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // get the num elements to be transfered
    Value *NumElts = CI.getOperand(2);

    // record load
    std::vector<Value *> args = make_vector(CallID, srcPointer, NumElts, 0);
    CallInst *recLoad = CallInst::Create(RecordLoad, args, "", &CI);

    // record store
    args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumLoads;
    ++NumStores;
    return true;
  }

  if (name == "pmem_memset_nodrain") {
    instrumentLock(&CI);

    // Get the destination pointer and cast it to a void pointer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = CI.getOperand(2);
    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // record store
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    // record flush
    args = make_vector<Value *>(CallID, dstPointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumStores;
    ++NumFlushes;
    return true;
  }

  if (name == "pmem_memset_persist") {
    instrumentLock(&CI);

    // Get the destination pointer and cast it to a void pointer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = CI.getOperand(2);
    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // record store
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    // record flush
    args = make_vector<Value *>(CallID, dstPointer, NumElts, 0);
    CallInst *recFlush = CallInst::Create(RecordFlushWrapper, args, "", &CI);

    // record fence
    args = make_vector<Value *>(CallID, 0);
    CallInst* recFence = CallInst::Create(RecordFence, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumStores;
    ++NumFlushes;
    ++NumFences;
    return true;
  }

  if (name == "pmem_memset") {
    instrumentLock(&CI);

    // Get the destination pointer and cast it to a void pointer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = CI.getOperand(2);
    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // record store
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumStores;
    return true;
  }

  return false;
}

bool TracingNoGiri::visitSpecialCall(CallInst &CI) {
  Function *CalledFunc = CI.getCalledFunction();

  // We do not support indirect calls to special functions.
  if (CalledFunc == nullptr)
    return false;

  // Do not consider a function special if it has a function body; in this
  // case, the programmer has supplied his or her version of the function, and
  // we will instrument it.
  if (!CalledFunc->isDeclaration())
    return false;

  // Check the name of the function against a list of known special functions.
  std::string name = CalledFunc->getName().str();
  if (name.substr(0,12) == "llvm.memset.") {
    instrumentLock(&CI);

    // Get the destination pointer and cast it to a void pointer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = CI.getOperand(2);
    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the external call instruction.
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);
    ++NumExtFuns; // Update statistics
    return true;
  } else if (name.substr(0,12) == "llvm.memcpy." ||
             name.substr(0,13) == "llvm.memmove." ||
             name == "strcpy") {
    instrumentLock(&CI);

    /* Record Load src, [CI] Load dst [CI] */
    // Get the destination and source pointers and cast them to void pointers.
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // Create the call to the run-time to record the loads and stores of
    // external call instruction.
    if(name == "strcpy") {
      // FIXME: If the tracer function should be inserted before or after????
      std::vector<Value *> args = make_vector(CallID, srcPointer, 0);
      CallInst::Create(RecordStrLoad, args, "", &CI);

      args = make_vector(CallID, dstPointer, 0);
      CallInst *recStore = CallInst::Create(RecordStrStore, args, "", &CI);
      instrumentUnlock(&CI);
      // Insert RecordStore after the instruction so that we can get the value
      CI.moveBefore(recStore);
    } else {
      // get the num elements to be transfered
      Value *NumElts = CI.getOperand(2);
      std::vector<Value *> args = make_vector(CallID, srcPointer, NumElts, 0);
      CallInst::Create(RecordLoad, args, "", &CI);

      args = make_vector(CallID, dstPointer, NumElts, 0);
      CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);
      instrumentUnlock(&CI);
      // Insert RecordStore after the instruction so that we can get the value
      CI.moveBefore(recStore);
    }

    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "strncpy" || name == "__strncpy_chk") {
    instrumentLock(&CI);
    /* Record Load src, [CI] Load dst [CI] */
    // Get the destination and source pointers and cast them to void pointers.
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer  = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    std::vector<Value *> args = make_vector(CallID, srcPointer, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    // TODO we assume the size is smaller than the dest size
    args = make_vector(CallID, dstPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "strcat") { /* Record Load dst, Load Src, Store dst-end before call inst  */
    instrumentLock(&CI);

    // Get the destination and source pointers and cast them to void pointers.
    Value *dstPointer = CI.getOperand(0);
    Value *srcPointer = CI.getOperand(1);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);

    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // Create the call to the run-time to record the loads and stores of
    // external call instruction.
    // CHECK: If the tracer function should be inserted before or after????
    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    args = make_vector(CallID, srcPointer, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    // Record the addresses before concat as they will be lost after concat
    args = make_vector(CallID, dstPointer, srcPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrcatStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);
    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "strlen") { /* Record Load */
    instrumentLock(&CI);

    // Get the destination and source pointers and cast them to void pointers.
    Value *srcPointer  = CI.getOperand(0);
    srcPointer  = castTo(srcPointer,  VoidPtrType, srcPointer->getName(), &CI);
    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    std::vector<Value *> args = make_vector(CallID, srcPointer, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    instrumentUnlock(&CI);
    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "strcmp") {
    instrumentLock(&CI);

    // Get the 2 pointers and cast them to void pointers.
    Value *strCmpPtr0 = CI.getOperand(0);
    Value *strCmpPtr1 = CI.getOperand(1);
    strCmpPtr0 = castTo(strCmpPtr0, VoidPtrType, strCmpPtr0->getName(), &CI);
    strCmpPtr1 = castTo(strCmpPtr1, VoidPtrType, strCmpPtr1->getName(), &CI);

    // Get the ID of the ext fun call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // Load for the first pointer
    std::vector<Value *> args = make_vector(CallID, strCmpPtr0, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    // Load for the second pointer
    args = make_vector(CallID, strCmpPtr1, 0);
    CallInst::Create(RecordStrLoad, args, "", &CI);

    instrumentUnlock(&CI);
    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "calloc") {
    instrumentLock(&CI);

    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = BinaryOperator::Create(BinaryOperator::Mul,
                                            CI.getOperand(0),
                                            CI.getOperand(1),
                                            "calloc par1 * par2",
                                            &CI);
    // Get the destination pointer and cast it to a void pointer.
    // Instruction * dstPointerInst;
    Value *dstPointer = castTo(&CI, VoidPtrType, CI.getName(), &CI);

    /* // To move after call inst, we need to know if cast is a constant expr or inst
    if ((dstPointerInst = dyn_cast<Instruction>(dstPointer))) {
        CI.moveBefore(dstPointerInst); // dstPointerInst->insertAfter(&CI);
        // ((Instruction *)NumElts)->insertAfter(dstPointerInst);
    }
    else {
        CI.moveBefore((Instruction *)NumElts); // ((Instruction *)NumElts)->insertAfter(&CI);
	}
    dstPointer = dstPointerInst; // Assign to dstPointer for instrn or non-instrn values
    */

    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    //
    // Create the call to the run-time to record the external call instruction.
    //
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);
    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore); //recStore->insertAfter((Instruction *)NumElts);
    // Moove cast, #byte computation and store to after call inst
    CI.moveBefore(cast<Instruction>(NumElts));

    ++NumExtFuns; // Update statistics
    return true;
  } else if (name == "tolower" || name == "toupper") {
    // Not needed as there are no loads and stores
  /*  } else if (name == "strncpy/itoa/stdarg/scanf/fscanf/sscanf/fread/complex/strftime/strptime/asctime/ctime") { */
  } else if (name == "fscanf") {
    // TODO
    // In stead of parsing format string, can we use the type of the arguments??
  } else if (name == "sscanf") {
    // TODO
  } else if (name == "sprintf") {
    instrumentLock(&CI);
    // Get the pointer to the destination buffer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);

    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // Scan through the arguments looking for what appears to be a character
    // string.  Generate load records for each of these strings.
    for (unsigned index = 2; index < CI.getNumOperands(); ++index) {
      if (CI.getOperand(index)->getType() == VoidPtrType) {
        // Create the call to the run-time to record the load from the string.
        // What about other loads??
        Value *Ptr = CI.getOperand(index);
        std::vector<Value *> args = make_vector(CallID, Ptr, 0);
        CallInst::Create(RecordStrLoad, args, "", &CI);

        ++NumLoadStrings; // Update statistics
      }
    }

    // Create the call to the run-time to record the external call instruction.
    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);
    ++NumStoreStrings; // Update statistics
    return true;
  } else if (name == "fgets") {
    instrumentLock(&CI);

    // Get the pointer to the destination buffer.
    Value * dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);

    // Get the ID of the ext fun call instruction.
    Value * CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // Create the call to the run-time to record the external call instruction.
    std::vector<Value *> args = make_vector(CallID, dstPointer, 0);
    CallInst *recStore = CallInst::Create(RecordStrStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);
    // Update statistics
    ++NumStoreStrings;
    return true;
  } else if (name == "snprintf") {
    instrumentLock(&CI);

    // Get the destination pointer and cast it to a void pointer.
    Value *dstPointer = CI.getOperand(0);
    dstPointer = castTo(dstPointer, VoidPtrType, dstPointer->getName(), &CI);
    // Get the number of bytes that will be written into the buffer.
    Value *NumElts = CI.getOperand(1);
    // Get the ID of the external funtion call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));

    // record store
    std::vector<Value *> args = make_vector(CallID, dstPointer, NumElts, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    instrumentUnlock(&CI);

    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);

    ++NumStores;
    return true;
  }

  return false;
}

bool TracingNoGiri::visitInlineAsm(CallInst &CI) {
  // InlineAsm conversion
  const InlineAsm *IA = cast<InlineAsm>(CI.getCalledValue());
  // Get ASM string
  const std::string& asm_str = IA->getAsmString();

  // These strings are inferred from: RECIPE, FAIR-FAST, CCEH, Level-Hashing
  // CLFLUSH and CLFLUSHOPT
  const std::string CLFLUSH ("clflush $0");
  // CLWB
  const std::string CLWB ("xsaveopt $0");
  // MFENCE
  const std::string MFENCE ("mfence");
  // XCHGQ
  const std::string XCHGQ ("xchgq $0,$1");

  // TODO: we only use a naive substring matching for both flush and fence.
  // If it is a flush
  if (asm_str.find(CLFLUSH) != std::string::npos ||
        asm_str.find(CLWB) != std::string::npos) {
    DEBUG(dbgs() << "FLUSH: " << asm_str);
    DEBUG(dbgs() << "; with #Args: " << CI.getNumArgOperands() << "\n");
    assert(CI.getNumArgOperands() == 1 || CI.getNumArgOperands() == 2
            && "FLUSH ASM's # of args is not 1 or 2!");

    // TODO: operand format (only use op 0) is only inferred from (see above);

    // Instrument the code and add RecordFLush
    instrumentLock(&CI);
    // Cast the pointer into a void pointer type.
    Value * Pointer = CI.getArgOperand(0);
    Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &CI);
    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> args=make_vector<Value *>(CallID, Pointer, 0);
    CallInst *recFlush = CallInst::Create(RecordFlush, args, "", &CI);
    instrumentUnlock(recFlush);

    CI.moveBefore(recFlush);

    // STATISTIC
    ++NumFlushes;
    return true;
  }

  // TODO: we only handle mfence here
  // If it is a mfence
  if (asm_str.find(MFENCE) != std::string::npos) {
    DEBUG(dbgs() << "MFENCE: " << asm_str);
    DEBUG(dbgs() << "; with #Args: " << CI.getNumArgOperands() << "\n");
    assert(CI.getNumArgOperands() == 0 && "FENCE ASM's # of args is not 0!");

    // Instrument the code and add RecordFence
    instrumentLock(&CI);
    // Get the ID of the call instruction.
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> args = make_vector<Value *>(CallID, 0);
    CallInst* recFence = CallInst::Create(RecordFence, args, "", &CI);
    instrumentUnlock(recFence);

    CI.moveBefore(recFence);

    ++NumFences;
    return true;
  }

  // TODO: hard-coded for now
  // If it is a xchgq
  if (asm_str.find(XCHGQ) != std::string::npos) {
    DEBUG(dbgs() << "XCHGQ: " << asm_str);
    DEBUG(dbgs() << "; with #Args: " << CI.getNumArgOperands() << "\n");
    assert(CI.getNumArgOperands() == 2 && "XCHGQ ASM's # of args is not 2!");

    instrumentLock(&CI);

    // Cast the pointer into a void pointer type.
    Value * Pointer = CI.getOperand(0);
    Pointer = castTo(Pointer, VoidPtrType, Pointer->getName(), &CI);
    // Get the size of the stored data.
    uint64_t size = TD->getTypeStoreSize(CI.getOperand(0)->getType());
    Value *StoreSize = ConstantInt::get(Int64Type, size);
    // Get the ID of the store instruction.
    Value *StoreID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    // Create the call to the run-time to record the store instruction.
    std::vector<Value *> args=make_vector<Value *>(StoreID, Pointer, StoreSize, 0);
    CallInst *recStore = CallInst::Create(RecordStore, args, "", &CI);

    instrumentUnlock(&CI);
    // Insert RecordStore after the instruction so that we can get the value
    CI.moveBefore(recStore);
    ++NumStores; // Update statistics

    return true;
  }

  return false;
}

void TracingNoGiri::visitCallInst(CallInst &CI) {
  // If it is InlienAsm, we check whether it is flush or fence
  if (isa<InlineAsm>(CI.getCalledValue())) {
     visitInlineAsm(CI);
     return;
   }

  // Attempt to get the called function.
  Function *CalledFunc = CI.getCalledFunction();
  if (!CalledFunc)
    return;

  // Do not instrument calls to tracing run-time functions or debug functions.
  if (isTracerFunction(CalledFunc))
    return;

  if (!CalledFunc->getName().str().compare(0,9,"llvm.dbg."))
    return;

  // Instrument external calls which can have invariants on its return value
  if (CalledFunc->isDeclaration() && CalledFunc->isIntrinsic()) {
     // Instrument special external calls which loads/stores
     // e.g. strlen(), strcpy(), memcpy() etc.
     visitSpecialCall(CI);
     return;
  }

  // If the called value is inline assembly code, then don't instrument it.
  if (isa<InlineAsm>(CI.getCalledValue()->stripPointerCasts()))
    return;

  // Function name de-mangling for c++
  std::string func_name_mangled = CalledFunc->getName().str();
  int status = -1;
  std::unique_ptr<char, void(*)(void*)> res
          { abi::__cxa_demangle(func_name_mangled.c_str(), NULL, NULL, &status),
            std::free };
  std::string FuncName;
  if (status == 0) {
    FuncName = res.get();
    FuncName = FuncName.substr(0, FuncName.length() - 2);
  } else {
    FuncName = func_name_mangled;
  }

  if (CalledFunc->isDeclaration() && FuncName.substr(0,5) == "pmem_") {
    if (visitPmemCall(CI, FuncName)) {
      return;
    }
  }
  if (CalledFunc->isDeclaration() && FuncName == "mmap") {
    instrumentLock(&CI);

    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    Value *ptr = &CI;
    Value *size = CI.getOperand(1);
    std::vector<Value *> args = make_vector(CallID, ptr, size, 0);
    CallInst *recInst = CallInst::Create(RecordMmap, args, "", &CI);

    instrumentUnlock(&CI);
    CI.moveBefore(recInst);
    return;
  }

  instrumentLock(&CI);
  // Get the ID of the store instruction.
  Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
  // Get the called function value and cast it to a void pointer.
  Value *FP = castTo(CI.getCalledValue(), VoidPtrType, "", &CI);

  // Check the function is ext tracing function or not
  std::vector<std::string>::iterator it = std::find(
                                                ext_tracing_func_vector.begin(),
                                                ext_tracing_func_vector.end(),
                                                FuncName);

  // ExtTracingFuncIndex will be put into the entry.length of a call entry
  // 0: means normal function
  // >0: means ext tracing function, index = ExtTracingFuncIndex - 1
  unsigned ExtTracingFuncIndex = 0;
  if (it != ext_tracing_func_vector.end()) {
    ExtTracingFuncIndex = std::distance(ext_tracing_func_vector.begin(),it) + 1;
    DEBUG(dbgs() << "ExtTracingFunc:" << FuncName << " : "
                 << ExtTracingFuncIndex << "\n");
  }
  Value *ExtTracingValue = ConstantInt::get(Int32Type, ExtTracingFuncIndex);
  // Create the call to the run-time to record the call instruction.
  std::vector<Value *> args =
                           make_vector<Value *>(CallID, FP, ExtTracingValue, 0);

  // Do not add calls to function call stack for external functions
  // as return records won't be used/needed for them, so call a special record function
  // FIXME!!!! Do we still need it after adding separate return records????
  Instruction *RC;
  if (CalledFunc->isDeclaration()) {
    RC = CallInst::Create(RecordExtCall, args, "", &CI);
  } else {
    RC = CallInst::Create(RecordCall, args, "", &CI);
  }
  instrumentUnlock(RC);

  // Create the call to the run-time to record the return of call instruction.
  CallInst *CallInst = CallInst::Create(RecordReturn, args, "", &CI);
  CI.moveBefore(CallInst);
  instrumentLock(CallInst);
  instrumentUnlock(CallInst);

  ++NumCalls; // Update statistics

  // Modeling pmemobj_tx_add_range
  if (FuncName == "pmemobj_tx_add_range") {
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    Value *oid_0 = CI.getOperand(0);
    Value *oid_1 = CI.getOperand(1);
    Value *off = CI.getOperand(2);
    Value *size = CI.getOperand(3);
    std::vector<Value *> args = make_vector(CallID, oid_0, oid_1, off, size, 0);
    Instruction* log = CallInst::Create(RecordTxAdd, args, "", CallInst);
    CallInst->moveBefore(log);
    return;
  }

  // Modeling pmemobj_tx_add_range_direct
  if (FuncName == "pmemobj_tx_add_range_direct") {
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    Value *ptr = CI.getOperand(0);
    Value *size = CI.getOperand(1);
    std::vector<Value *> args = make_vector(CallID, ptr, size, 0);
    Instruction* log = CallInst::Create(RecordTxAddDirect, args, "", CallInst);
    CallInst->moveBefore(log);
    return;
  }

  // Modeling pmemobj_tx_zalloc
  if (FuncName == "pmemobj_tx_alloc" || FuncName == "pmemobj_tx_zalloc") {
    Value *CallID = ConstantInt::get(Int32Type, lsNumPass->getID(&CI));
    Value *size = CI.getOperand(0);
    std::vector<Value *> args = make_vector(CallID, &CI, size, 0);
    Instruction* log = CallInst::Create(RecordTxAlloc, args, "", CallInst);
    CallInst->moveBefore(log);
    return;
  }

  // The best way to handle external call is to set a flag before calling ext fn and
  // use that to determine if an internal function is called from ext fn. It flag can be
  // reset afterwards and restored to its original value before returning to ext code.
  // FIXME!!!! LATER

#if 0
  if (CalledFunc->isDeclaration() &&
      CalledFunc->getName().str() == "pthread_create") {
    // If pthread_create is called then handle it specially as it calls
    // functions externally and add an extra call for the externally
    // called functions with the same id so that returns can match with it.
    // In addition to a function call to pthread_create.
    // Get the external function pointer operand and cast it to a void pointer
    Value *FP = castTo(CI.getOperand(2), VoidPtrType, "", &CI);
    // Create the call to the run-time to record the call instruction.
    std::vector<Value *> argsExt = make_vector<Value *>(CallID, FP, 0);
    CallInst = CallInst::Create(RecordCall, argsExt, "", &CI);
    CI.moveBefore(CallInst);

    // Update statistics
    ++Calls;

    // For, both external functions and internal/ext functions called from
    // external functions, return records are not useful as they won't be used.
    // Since, we won't create return records for them, simply update the call
    // stack to mark the end of function call.

    //args = make_vector<Value *>(CallID, FP, 0);
    //CallInst::Create(RecordExtCallRet, args.begin(), args.end(), "", &CI);

    // Create the call to the run-time to record the return of call instruction.
    CallInst::Create(RecordReturn, argsExt, "", &CI);
  }
#endif

  // Instrument special external calls which loads/stores
  // like strlen, strcpy, memcpy etc.
  visitSpecialCall(CI);
}

bool TracingNoGiri::runOnBasicBlock(BasicBlock &BB) {
  // Fetch the analysis results for numbering basic blocks.
  // Will be run once per module
  //TD        = &getAnalysis<DataLayout>();
  TD = &(BB.getParent()->getParent()->getDataLayout());
  bbNumPass = &getAnalysis<QueryBasicBlockNumbers>();
  lsNumPass = &getAnalysis<QueryLoadStoreNumbers>();

  // Instrument the basic block so that it records its execution.
  instrumentBasicBlock(BB);

  // Scan through all instructions in the basic block and instrument them as
  // necessary.  Use a worklist to contain the instructions to avoid any
  // iterator invalidation issues when adding instructions to the basic block.
  std::vector<Instruction *> Worklist;
  for (BasicBlock::iterator I = BB.begin(); I != BB.end(); ++I)
    Worklist.push_back(&*I);
  visit(Worklist.begin(), Worklist.end());

  instrumentMainEntryBB(BB);

  // Update the number of basic blocks with phis.
  if (hasPHI(BB))
    ++NumPHIBBs;

  // Update the number of basic blocks.
  ++NumBBs;

  // Assume that we modified something.
  return true;
}
