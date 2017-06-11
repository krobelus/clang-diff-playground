//===- ClangDiff.h   - compare source files by AST nodes ------*- C++ -*- -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DIFF_CLANGDIFF_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DIFF_CLANGDIFF_H

#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/PriorityQueue.h"

#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace clang;
using namespace ast_type_traits;

namespace clang {
namespace diff {

using NodeId = int;

const NodeId NoNodeId = -1;

struct Node {
  NodeId Parent, LeftMostDescendant;
  int Depth;
  DynTypedNode ASTNode;
  SmallVector<NodeId, 4> Children;

  ASTNodeKind getType() const { return ASTNode.getNodeKind(); }
  bool hasSameType(const Node &Other) const {
    return getType().isSame(Other.getType()) ||
           (getType().isNone() && Other.getType().isNone());
  }
  const StringRef getTypeLabel() const { return getType().asStringRef(); }
};

struct TreeRoot {
  int NodeCount;
  int MaxDepth = 0;
  std::unique_ptr<ASTUnit> ASTUnit;
  std::vector<Node> Postorder;

  NodeId root() const {
    return Postorder.empty() ? NoNodeId : Postorder.size() - 1;
  }
  int numberOfDescendants(NodeId Id) const {
    return Id - Postorder[Id].LeftMostDescendant;
  }
  std::string label(NodeId Id) const {
    const Node &N = Postorder[Id];
    SourceRange Range = N.ASTNode.getSourceRange();
    if (Range.isValid()) {
      const SourceManager &SM = ASTUnit->getSourceManager();
      const LangOptions &LangOpts = ASTUnit->getLangOpts();
      SourceLocation LocStart = Range.getBegin();
      SourceLocation LocEnd =
          Lexer::getLocForEndOfToken(Range.getEnd(), 0, SM, LangOpts);
      const char *Begin = SM.getCharacterData(LocStart);
      const char *End = SM.getCharacterData(LocEnd);
      return std::string(Begin, End - Begin);
    }
    return "";
  }
  void dump() const {
    for (NodeId Id = root(); Id >= 0; Id--) {
      const Node &N = Postorder[Id];
      llvm::errs() << "type: " << N.getTypeLabel() << R"(, label: ")"
                   << label(Id) << "\"\n";
    }
  }
};

namespace {
struct CompareDepth {
  const TreeRoot &Tree;
  CompareDepth(const TreeRoot &Tree) : Tree(Tree) {}
  bool operator()(NodeId id1, NodeId id2) const {
    return Tree.Postorder[id1].Depth > Tree.Postorder[id2].Depth;
  }
};
} // namespace

class PriorityList {
  const TreeRoot &Tree;
  CompareDepth Comparator;
  std::vector<NodeId> Container;
  PriorityQueue<NodeId, std::vector<NodeId>, CompareDepth> List;

public:
  PriorityList(const TreeRoot &Tree)
      : Tree(Tree), Comparator(Tree), List(Comparator, Container) {
    Container.reserve(Tree.Postorder.size());
    for (NodeId Id = 0; Id < Tree.NodeCount; Id++) {
      push(Id);
    }
  }

  void push(NodeId id) { List.push(id); }

  std::vector<NodeId> pop() {
    int Max = peekMax();
    std::vector<NodeId> Result;
    if (Max != 0) {
      while (peekMax() == Max) {
        Result.push_back(List.top());
        List.pop();
      }
    }
    return Result;
  }

  int peekMax() const {
    if (List.empty()) {
      return 0;
    }
    return 1 + Tree.MaxDepth - Tree.Postorder[List.top()].Depth;
  }

  void open(NodeId id) {
    for (NodeId Child : Tree.Postorder[id].Children) {
      push(Child);
    }
  }
};

class MappingList : std::vector<std::pair<NodeId, NodeId>> {
  std::unordered_set<NodeId> mappedSources;
  std::unordered_set<NodeId> mappedDestinations;

public:
  void add(NodeId src, NodeId dst) {
    assert(!mappedSources.count(src) && !mappedDestinations.count(dst));
    emplace_back(std::make_pair(src, dst));
  }
  bool hasSrc(NodeId src) const { return mappedSources.count(src) != 0u; }
  bool hasDst(NodeId dst) const { return mappedDestinations.count(dst) != 0u; }
};

class Mappings {
  NodeId *src2dst, *dst2src;

public:
  Mappings(const TreeRoot &T1, const TreeRoot &T2) {
    src2dst = new NodeId[T1.NodeCount];
    dst2src = new NodeId[T2.NodeCount];
    // set everything to -1
    memset(src2dst, NoNodeId, T1.NodeCount * sizeof(*src2dst));
    memset(dst2src, NoNodeId, T2.NodeCount * sizeof(*dst2src));
  }
  ~Mappings() {
    delete[] src2dst;
    delete[] dst2src;
  }
  void link(NodeId src, NodeId dst) {
    src2dst[src] = dst;
    dst2src[dst] = src;
  }
  NodeId getDst(NodeId src) const { return src2dst[src]; }
  NodeId getSrc(NodeId dst) const { return dst2src[dst]; }
  bool hasSrc(NodeId src) const { return src2dst[src] != NoNodeId; }
  bool hasDst(NodeId dst) const { return dst2src[dst] != NoNodeId; }
};

class ZsMatcher {
  const TreeRoot &T1;
  const TreeRoot &T2;

public:
  ZsMatcher(const TreeRoot &T1, const TreeRoot &T2) : T1(T1), T2(T2) {}

  std::vector<std::pair<NodeId, NodeId>> match() { return {}; }
};

} // namespace diff
} // namespace clang

#endif
