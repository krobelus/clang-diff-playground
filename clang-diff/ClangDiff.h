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

#include "clang/AST/Expr.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/PriorityQueue.h"
#include "llvm/Support/FormatVariadic.h"

#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace clang;
using namespace ast_type_traits;

namespace clang {
namespace diff {

// Given a tree, this identifies a node by its postorder offset.
using NodeId = int;

// Sentinel value for invalid nodes.
const NodeId NoNodeId = -1;

// Represents a Clang AST node, alongside some additional information.
struct Node {
  NodeId Parent, LeftMostDescendant;
  int Depth;
  DynTypedNode ASTNode;
  // Maybe there is a better way to store children than this.
  SmallVector<NodeId, 4> Children;

  ASTNodeKind getType() const { return ASTNode.getNodeKind(); }
  bool hasSameType(const Node &Other) const {
    return getType().isSame(Other.getType()) ||
           (getType().isNone() && Other.getType().isNone());
  }
  const StringRef getTypeLabel() const { return getType().asStringRef(); }

  void dump(NodeId Id = NoNodeId) const {
    for (int i = 0; i < Depth; ++i) {
      llvm::errs() << " ";
    }
    llvm::errs() << getTypeLabel() << "(" << Id << ")\n";
  }
};

// Wrapper for the Clang AST of a TranslationUnit.
struct TreeRoot {
  int NodeCount, LeafCount;
  int MaxDepth = 0;
  std::unique_ptr<ASTUnit> ASTUnit;
  std::vector<Node> Postorder;
  std::vector<NodeId> KeyRoots;

  NodeId root() const {
    return Postorder.empty() ? NoNodeId : Postorder.size() - 1;
  }
  int numberOfDescendants(NodeId Id) const {
    return Id - Postorder[Id].LeftMostDescendant;
  }
  std::string label(NodeId Id) const {
    const Node &N = Postorder[Id];
    const DynTypedNode &DTN = N.ASTNode;
    if (0) {
    } else if (auto *X = DTN.get<BinaryOperator>()) {
      return X->getOpcodeStr();
    } else if (auto *X = DTN.get<IntegerLiteral>()) {
      auto tmp = X->getValue();
      SmallString<256> Str;
      tmp.toString(Str, /*Radix=*/10, /*Signed=*/false);
      return Str.str();
    } else if (auto *X = DTN.get<CompoundStmt>()) {
      return "";
    } else if (auto *X = DTN.get<DeclStmt>()) {
      return "";
    } else if (auto *X = DTN.get<DeclRefExpr>()) {
      return "";
    } else if (auto *D = DTN.get<Decl>()) {
      /* llvm::errs() << "got Decl\n"; */
    } else if (auto *S = DTN.get<Stmt>()) {
      /* llvm::errs() << "got Stmt\n"; */
    } else if (auto *T = DTN.get<QualType>()) {
      /* llvm::errs() << "got QualType\n"; */
    } else {
      llvm::errs() << "fixme: " << N.getTypeLabel() << "\n";
    }
    return "";
  }
  std::string getSubtreeString(NodeId Id) const {
    std::string StringRepresentation;
    raw_string_ostream OS(StringRepresentation);
    PrintingPolicy PP(/*LangOpts=*/ASTUnit->getLangOpts());
    const Node &N = Postorder[Id];
    N.ASTNode.print(OS, PP);
    return OS.str();
  }
  void dump(NodeId Id = NoNodeId) const {
    if (Id == NoNodeId) {
      Id = root();
    }
    const Node &N = Postorder[Id];
    N.dump(Id);
    for (NodeId Child : N.Children) {
      dump(Child);
    }
  }
  std::string showNode(NodeId Id) const {
    if (Id == NoNodeId) {
      return "None";
    }
    return formatv("{0}({1}):{2}", Postorder[Id].getTypeLabel(), Id,
        label(Id));
  }
  void computeKeyRoots() {
    KeyRoots.resize(LeafCount);
    std::unordered_set<NodeId> Visited;
    NodeId K = LeafCount - 1;
    for (NodeId Id = root(); Id >= 0; --Id) {
      NodeId LeftDesc = Postorder[Id].LeftMostDescendant;
      if (Visited.count(LeftDesc)) {
        continue;
      }
      KeyRoots[K] = Id;
      Visited.insert(LeftDesc);
      --K;
    }
  }
};

namespace {
// Compares nodes by their depth.
struct CompareDepth {
  const TreeRoot &Tree;
  CompareDepth(const TreeRoot &Tree) : Tree(Tree) {}
  bool operator()(NodeId id1, NodeId id2) const {
    return Tree.Postorder[id1].Depth > Tree.Postorder[id2].Depth;
  }
};
} // namespace

// Priority queue for nodes, sorted by their depth.
class PriorityList {
  const TreeRoot &Tree;
  CompareDepth Comparator;
  std::vector<NodeId> Container;
  PriorityQueue<NodeId, std::vector<NodeId>, CompareDepth> List;

public:
  PriorityList(const TreeRoot &Tree)
      : Tree(Tree), Comparator(Tree), List(Comparator, Container) {
    Container.reserve(Tree.Postorder.size());
    for (NodeId Id = 0; Id < Tree.NodeCount; ++Id) {
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

#if 0
class MappingList : std::vector<std::pair<NodeId, NodeId>> {
  std::unordered_set<NodeId> mappedSources;
  std::unordered_set<NodeId> mappedDestinations;

public:
  void add(NodeId src, NodeId dst) {
    assert(!mappedSources.count(src) && !mappedDestinations.count(dst));
    emplace_back(src, dst);
  }
  bool hasSrc(NodeId src) const { return mappedSources.count(src) != 0u; }
  bool hasDst(NodeId dst) const { return mappedDestinations.count(dst) != 0u; }
};
#endif

// Maps nodes of the left tree to ones on the right, and vice versa.
// Supports fast insertion and lookup but not traversal.
// maybe it is better to just use std::map
class Mappings {
  NodeId *src2dst, *dst2src;
  const TreeRoot &T1, &T2;
  size_t size;

public:
  Mappings(const TreeRoot &T1, const TreeRoot &T2) : T1(T1), T2(T2) {
    size = T1.NodeCount + T2.NodeCount;
    src2dst = new NodeId[size];
    dst2src = new NodeId[size];
    // set everything to -1
    memset(src2dst, NoNodeId, size * sizeof(*src2dst));
    memset(dst2src, NoNodeId, size * sizeof(*dst2src));
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

  void dump() const {
    for (NodeId Id1 = 0, Id2; Id1 < T1.NodeCount; ++Id1) {
      if ((Id2 = getDst(Id1)) != NoNodeId) {
        llvm::errs() << "Match " << T1.Postorder[Id1].getTypeLabel() << " ("
                     << Id1 << ")"
                     << " to " << T2.Postorder[Id2].getTypeLabel() << " ("
                     << Id2 << ")"
                     << "\n";
      }
    }
  }
};

// Computes an optimal mapping between two trees
class ZsMatcher {
  using Pairs = std::vector<std::pair<NodeId, NodeId>>;

private:
  const TreeRoot &T1;
  const TreeRoot &T2;
  double **TreeDist;
  double **ForestDist;

  void computeTreeDist() {
    for (NodeId Id1 : T1.KeyRoots) {
      for (NodeId Id2 : T2.KeyRoots) {
        computeForestDist(Id1 + 1, Id2 + 1);
      }
    }
  }

  double getUpdateCost(NodeId Id1, NodeId Id2) {
    const Node &N1 = T1.Postorder[Id1];
    const Node &N2 = T2.Postorder[Id2];
    if (!N1.hasSameType(N2)) {
      return INFINITY;
    }
    if (T1.label(Id1) == T2.label(Id2)) {
      return 0;
    }
    // TODO Use string editing distance if applicable.
    return 1;
  }

  void computeForestDist(NodeId Id1, NodeId Id2) {
    const Node &N1 = T1.Postorder[Id1 - 1];
    const Node &N2 = T2.Postorder[Id2 - 1];

    NodeId LMD1 = N1.LeftMostDescendant;
    NodeId LMD2 = N2.LeftMostDescendant;

    ForestDist[LMD1][LMD2] = 0.0;
    for (NodeId D1 = LMD1 + 1; D1 <= Id1; ++D1) {
      double DeletionCost = 1.0;
      ForestDist[D1][LMD2] = ForestDist[D1 - 1][LMD2] + DeletionCost;
      for (NodeId D2 = LMD2 + 1; D2 <= Id2; ++D2) {
        double InsertionCost = 1.0;
        ForestDist[LMD1][D2] = ForestDist[LMD1][D2 - 1] + InsertionCost;
        if (T1.Postorder[D1 - 1].LeftMostDescendant == LMD1 &&
            T2.Postorder[D2 - 1].LeftMostDescendant == LMD2) {
          double UpdateCost = getUpdateCost(D1, D2);
          ForestDist[D1][D2] =
              std::min(std::min(ForestDist[D1 - 1][D2] + DeletionCost,
                                ForestDist[D1][D2 - 1] + InsertionCost),
                       ForestDist[D1 - 1][D2 - 1] + UpdateCost);
          TreeDist[D1][D2] = TreeDist[D1][D2];
        } else {
          ForestDist[D1][D2] =
              std::min(std::min(ForestDist[D1 - 1][D2] + DeletionCost,
                                ForestDist[D1][D2 - 1] + InsertionCost),
                       ForestDist[LMD1][LMD2] + TreeDist[D1][D2]);
        }
      }
    }
  }

public:
  ZsMatcher(const TreeRoot &T1, const TreeRoot &T2) : T1(T1), T2(T2) {
    TreeDist = new double *[T1.NodeCount + 1];
    ForestDist = new double *[T1.NodeCount + 1];
    for (int i = 0; i < T1.NodeCount + 1; ++i) {
      TreeDist[i] = new double[T2.NodeCount + 1]();
      ForestDist[i] = new double[T2.NodeCount + 1]();
    }
  }

  ~ZsMatcher() {
    for (int i = 0; i < T1.NodeCount + 1; ++i) {
      delete[] TreeDist[i];
      delete[] ForestDist[i];
    }
    delete[] TreeDist;
    delete[] ForestDist;
  }

  Pairs match() {
    Pairs Matches;

    computeTreeDist();

    bool RootNodePair = true;
    Pairs TreePairs{{T1.NodeCount, T2.NodeCount}};

    NodeId LastRow, LastCol, FirstRow, FirstCol, Row, Col;
    while (TreePairs.size() != 0) {
      std::tie(LastRow, LastCol) = TreePairs.back();
      TreePairs.pop_back();

      if (!RootNodePair) {
        computeForestDist(LastRow, LastCol);
      }

      RootNodePair = false;

      FirstRow = T1.Postorder[LastRow - 1].LeftMostDescendant;
      FirstCol = T2.Postorder[LastCol - 1].LeftMostDescendant;

      Row = LastRow;
      Col = LastCol;

      while (Row > FirstRow || Col > FirstCol) {
        if (Row > FirstRow &&
            ForestDist[Row - 1][Col] + 1 == ForestDist[Row][Col]) {
          --Row;
        } else if (Col > FirstCol &&
                   ForestDist[Row][Col - 1] + 1 == ForestDist[Row][Col]) {
          --Col;
        } else {
          NodeId LMD1 = T1.Postorder[Row - 1].LeftMostDescendant;
          NodeId LMD2 = T2.Postorder[Col - 1].LeftMostDescendant;
          if (LMD1 == T1.Postorder[LastRow - 1].LeftMostDescendant &&
              LMD2 == T2.Postorder[LastCol - 1].LeftMostDescendant) {
            Matches.emplace_back(Row - 1, Col - 1);
            --Row;
            --Col;
          } else {
            TreePairs.emplace_back(Row, Col);
            Row = LMD1;
            Col = LMD2;
          }
        }
      }
    }

    return Matches;
  }
};

} // namespace diff
} // namespace clang

#endif
