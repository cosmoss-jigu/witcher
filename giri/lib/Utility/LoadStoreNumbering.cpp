//===- LoadStoreNumbering.cpp - Provide BB identifiers ----------*- C++ -*-===//
//
//                     Giri: Dynamic Slicing in LLVM
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that assigns a unique ID to each basic block.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "giriutil"

#define MAX_PROGRAM_POINTS 2000000

#include "Utility/LoadStoreNumbering.h"
#include "Utility/Debug.h"
#include "Utility/SourceLineMapping.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>

using namespace dg;
using namespace llvm;

//===----------------------------------------------------------------------===//
//                        Command Line Arguments
//===----------------------------------------------------------------------===//
// The trace filename was specified externally in tracing part
static cl::opt<bool>
DumpID("dump-lsid", cl::desc("Dump assigned Load/Store ID"), cl::init(false));

//===----------------------------------------------------------------------===//
//                        Load Store Number Passes
//===----------------------------------------------------------------------===//
char LoadStoreNumberPass::ID    = 0;
char QueryLoadStoreNumbers::ID  = 0;
char RemoveLoadStoreNumbers::ID = 0;

static const char *mdKindName = "dgls";

static RegisterPass<LoadStoreNumberPass>
X ("lsnum", "Assign Unique Identifiers to Loads and Stores");

static RegisterPass<RemoveLoadStoreNumbers>
Z ("remove-lsnum", "Remove Unique Identifiers of Loads and Stores");

static RegisterPass<QueryLoadStoreNumbers>
Y ("query-lsnum", "Query Unique Identifiers of Loads and Stores");

MDNode *LoadStoreNumberPass::assignID(Instruction *I, unsigned id) {
  if (DumpID) {
    dbgs() << id << " : ";
    I->print(dbgs());
    dbgs() << "\n";
  }

  // Fetch the context in which the enclosing module was defined.  We'll need
  // it for creating practically everything.
  Module *M = I->getParent()->getParent()->getParent();
  LLVMContext &Context = M->getContext();

  // Create a new metadata node that contains the ID as a constant.
  /*
  Value *ID[2];
  ID[0] = I;
  ID[1] = ConstantInt::get(Type::getInt32Ty(Context), id);
  return MDNode::getWhenValsUnresolved(Context, ArrayRef<Value*>(ID, 2), false);
  */
  Metadata *meta[2];
  meta[0] = ValueAsMetadata::get(I);
  meta[1] = ValueAsMetadata::get(ConstantInt::get(Type::getInt32Ty(Context),
                                                  id));
  return MDNode::get(Context, ArrayRef<Metadata*>(meta, 2));
}

bool LoadStoreNumberPass::runOnModule(Module &M) {
  // Now create a named metadata node that links all of this metadata together.
  MD = M.getOrInsertNamedMetadata(mdKindName);

  // Scan through the module and assign a unique, positive (i.e., non-zero) ID
  // to every load and store instruction.  Create an array of metadata nodes
  // to hold this data.
  count = 0;
  visit(&M);
  DEBUG(dbgs() << "Number of monitored program points: " << count << "\n");
  if (count > MAX_PROGRAM_POINTS)
    errs() << "Number of monitored program points exceeds maximum value.\n";
  return true;
}

bool QueryLoadStoreNumbers::runOnModule(Module &M) {
  DEBUG(dbgs() << "Inside QueryLoadStoreNumbers for module "
               << M.getModuleIdentifier()
               << "\n");
  // Get the basic block metadata. If there isn't any metadata, then no basic
  // block has been numbered.
  const NamedMDNode *MD = M.getNamedMetadata(mdKindName);
  if (!MD)
    return false;

  // Scan through all of the metadata (should be pairs of instructions/IDs) and
  // bring them into our internal data structure.
  for (unsigned index = 0; index < MD->getNumOperands(); ++index) {
    // The instruction should be the first element, and the ID should be the
    // second element.
    MDNode *Node = dyn_cast<MDNode>(MD->getOperand (index));
    assert(Node && "Wrong type of meta data!\n");
    //Instruction *I = dyn_cast<Instruction>(Node->getOperand(0));
    //ConstantInt *ID = dyn_cast<ConstantInt>(Node->getOperand(1));
    Instruction *I = dyn_cast<Instruction>((dyn_cast<ValueAsMetadata>(Node->getOperand(0)))->getValue());
    ConstantInt *ID = dyn_cast<ConstantInt>((dyn_cast<ValueAsMetadata>(Node->getOperand(1)))->getValue());

    // Do some assertions to make sure that everything is sane.
    assert(I && "MDNode first element is not an Instruction!\n");
    assert(ID && "MDNode second element is not a ConstantInt!\n");

    // Add the values into the map.
    assert(ID->getZExtValue() && "Instruction with zero ID!\n");
    IDMap[I] = ID->getZExtValue();
    unsigned id = (unsigned)ID->getZExtValue();
    bool inserted = InstMap.insert(std::make_pair(id,I)).second;
    assert(inserted && "Repeated identifier!\n");
  }

  /*
  for (auto i : IDMap) {
    i.first->print(dbgs());
    dbgs() << "\nls_id: " << i.second;
    dbgs() << "\nsrc_info:"
           << SourceLineMappingPass::locateSrcInfo((Instruction*)i.first)
           << "\n";
  }
  */

  return false;
}

bool RemoveLoadStoreNumbers::runOnModule(Module &M) {
  // Get the basic block metadata. If there isn't any metadata, then no basic
  // blocks have been numbered.
  NamedMDNode *MD = M.getNamedMetadata(mdKindName);
  if (!MD)
    return false;

  // Remove the metadata.
  MD->eraseFromParent();

  // Assume we always modify the module.
  return true;
}
