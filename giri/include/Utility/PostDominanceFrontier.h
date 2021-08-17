//=- llvm/Analysis/PostDominanceFrontier.h - Post Dominance Frontier Calculation-*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file exposes interfaces to post dominance frontier information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H
#define LLVM_ANALYSIS_POST_DOMINANCE_FRONTIER_H

#include <iostream>

#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Pass.h"

namespace llvm {

/// PostDominanceFrontier Class - Concrete subclass of DominanceFrontier that is
/// used to compute the a post-dominance frontier.
struct PostDominanceFrontier : public DominanceFrontierBase<BasicBlock, true>,
                               public FunctionPass {
  static char ID;
  PostDominanceFrontier()
    : FunctionPass(ID), DominanceFrontierBase() {
    //initializePostDominanceFrontierPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnFunction(Function &F) {
    Frontiers.clear();
    // PostDominatorTree &DT = getAnalysis<PostDominatorTree>();
    PostDominatorTree &DT = getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
    // Roots = DT.getRoots();
    Roots.clear();
    for (auto root : DT.getRoots()) {
      Roots.push_back(root);
    }
    if (const DomTreeNode *Root = DT.getRootNode())
      calculate(DT, Root);
    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    // AU.addRequired<PostDominatorTree>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
  }

private:
  const DomSetType &calculate(const PostDominatorTree &DT,
                              const DomTreeNode *Node);
};

FunctionPass* createPostDomFrontier();

} // End llvm namespace

#endif
