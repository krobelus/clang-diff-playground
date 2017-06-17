//===- ClangDiff.h   - compare source files by AST nodes ------*- C++ -*- -===//
//
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

#include <limits>
#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace clang;
using namespace ast_type_traits;

namespace clang {
namespace diff {

// Within a tree, this identifies a node by its postorder offset.
using NodeId = int;

// Sentinel value for invalid nodes.
const NodeId NoNodeId = -1;

// Represents a Clang AST node, alongside some additional information.
struct Node {
  NodeId Parent, LeftMostDescendant;
  int Depth, Height;
  DynTypedNode ASTNode;
  // Maybe there is a better way to store children than this.
  SmallVector<NodeId, 4> Children;

  ASTNodeKind getType() const { return ASTNode.getNodeKind(); }
  bool hasSameType(const Node &Other) const {
    return getType().isSame(Other.getType()) ||
           (getType().isNone() && Other.getType().isNone());
  }
  const StringRef getTypeLabel() const { return getType().asStringRef(); }
  bool isLeaf() const { return Children.empty(); }
};

// Represents the AST of a TranslationUnit.
class TreeRoot {
private:
  std::vector<Node> Postorder;

public:
  int LeafCount = 0;
  int MaxDepth = 0;
  std::unique_ptr<ASTUnit> ASTUnit;

  void setSize(size_t Size) { Postorder.resize(Size); }
  int getSize() const { return Postorder.size(); }
  NodeId root() const { return getSize() - 1; }

  const Node &getNode(NodeId Id) const { return Postorder[Id]; }
  Node &getMutableNode(NodeId Id) { return Postorder[Id]; }
  bool isValidNodeId(NodeId Id) const { return Id >= 0 && Id < getSize(); }
  void addNode(Node &N) { Postorder.emplace_back(std::move(N)); }

  // Returns the number of leaves in the subtree.
  int getSubtreePostorder(std::vector<NodeId> &Subtree, NodeId Root) const {
    int Leaves = 0;
    std::function<void(NodeId)> Traverse = [&](NodeId Id) {
      const Node &N = getNode(Id);
      for (NodeId Child : N.Children) {
        Traverse(Child);
      }
      if (N.isLeaf()) {
        ++Leaves;
      }
      Subtree.push_back(Id);
    };
    Traverse(Root);
    return Leaves;
  }
  std::vector<NodeId> getSubtreeBfs(NodeId Id) const {
    std::vector<NodeId> Subtree;
    size_t Expanded = 0;
    Subtree.push_back(Id);
    while (Expanded < Subtree.size()) {
      for (NodeId Child : getNode(Subtree[Expanded++]).Children) {
        Subtree.push_back(Child);
      }
    }
    return Subtree;
  }

  int numberOfDescendants(NodeId Id) const {
    return Id - getNode(Id).LeftMostDescendant;
  }
  std::string label(NodeId Id) const {
    assert(isValidNodeId(Id));
    const Node &N = getNode(Id);
    const DynTypedNode &DTN = N.ASTNode;
    if (false) {
    } else if (auto *X = DTN.get<FieldDecl>()) {
      return X->getName();
    } else if (auto *X = DTN.get<BinaryOperator>()) {
      return X->getOpcodeStr();
    } else if (auto *X = DTN.get<IntegerLiteral>()) {
      auto Tmp = X->getValue();
      SmallString<256> Str;
      Tmp.toString(Str, /*Radix=*/10, /*Signed=*/false);
      return Str.str();
    } else if (auto *X = DTN.get<CompoundStmt>()) {
      return "";
    } else if (auto *X = DTN.get<DeclStmt>()) {
      return "";
    } else if (auto *X = DTN.get<DeclRefExpr>()) {
      return "";
    } else if (auto *D = DTN.get<Decl>()) {
    } else if (auto *S = DTN.get<Stmt>()) {
    } else if (auto *T = DTN.get<QualType>()) {
    }
    llvm_unreachable("Fatal: unhandled AST node\n");
  }
  void dumpTree(NodeId Id = NoNodeId) const {
    if (Id == NoNodeId) {
      Id = root();
    }
    const Node &N = getNode(Id);
    for (int I = 0; I < N.Depth; ++I) {
      outs() << " ";
    }
    outs() << showNode(Id) << "\n";
    for (NodeId Child : N.Children) {
      dumpTree(Child);
    }
  }
  std::string showNode(NodeId Id) const {
    if (Id == NoNodeId) {
      return "None";
    }
    std::string Label;
    if (label(Id) != "") {
      Label = formatv(": {0}", label(Id));
    }
    return formatv("{0}{1}({2})", getNode(Id).getTypeLabel(), Label, Id);
  }
  void dumpNodeAsJson(NodeId Id) const {
    auto N = getNode(Id);
    std::string Label;
    if (label(Id) != "") {
      Label = formatv(R"(,"label":"{0}")", label(Id));
    }
    outs() << formatv(R"({"typeLabel":"{0}"{1},"children":[)", N.getTypeLabel(),
                      Label);
    if (N.Children.size() > 0) {
      dumpNodeAsJson(N.Children[0]);
      for (size_t I = 1; I < N.Children.size(); ++I) {
        outs() << ",";
        dumpNodeAsJson(N.Children[I]);
      }
    }
    outs() << "]}";
  }
  void dumpAsJson() const {
    outs() << R"({"root":)";
    dumpNodeAsJson(root());
    outs() << "}";
  }
};

namespace {
// Compares nodes by their depth.
struct HeightLess {
  const TreeRoot &Tree;
  HeightLess(const TreeRoot &Tree) : Tree(Tree) {}
  bool operator()(NodeId Id1, NodeId Id2) const {
    return Tree.getNode(Id1).Height < Tree.getNode(Id2).Height;
  }
};
} // namespace

// Priority queue for nodes, sorted descendingly by their height.
class PriorityList {
  const TreeRoot &Tree;
  HeightLess Comparator;
  std::vector<NodeId> Container;
  PriorityQueue<NodeId, std::vector<NodeId>, HeightLess> List;

public:
  PriorityList(const TreeRoot &Tree)
      : Tree(Tree), Comparator(Tree), List(Comparator, Container) {}

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
    // TODO this is here to get a stable output, not a good heuristic
    std::sort(Result.begin(), Result.end());
    return Result;
  }

  int peekMax() const {
    if (List.empty()) {
      return 0;
    }
    return Tree.getNode(List.top()).Height;
  }

  void open(NodeId Id) {
    for (NodeId Child : Tree.getNode(Id).Children) {
      push(Child);
    }
  }
};

// Maps nodes of the left tree to ones on the right, and vice versa.
// Supports fast insertion and lookup.
// TODO allow multiple mappings
class Mappings {
  NodeId *src2dst, *dst2src;
  const TreeRoot &T1, &T2;

public:
  Mappings(const TreeRoot &T1, const TreeRoot &T2) : T1(T1), T2(T2) {
    int Size = T1.getSize() + T2.getSize();
    src2dst = new NodeId[Size];
    dst2src = new NodeId[Size];
    // set everything to NoNodeId == -1
    memset(src2dst, NoNodeId, Size * sizeof(*src2dst));
    memset(dst2src, NoNodeId, Size * sizeof(*dst2src));
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

  void dumpMapping() const {
    for (NodeId Id1 = 0, Id2; Id1 < T1.getSize(); ++Id1) {
      if ((Id2 = getDst(Id1)) != NoNodeId) {
        outs() << formatv("Match {0} to {1}\n", T1.showNode(Id1),
                          T2.showNode(Id2));
      }
    }
  }
};

class Subtree {
  using SNodeId = int;
  const TreeRoot &Tree;
  std::vector<NodeId> OriginalIds;
  std::vector<SNodeId> LeftMostDescendants;
  int LeafCount;

public:
  std::vector<NodeId> KeyRoots;

  Subtree(const TreeRoot &Tree, NodeId RootId) : Tree(Tree) {
    LeafCount = Tree.getSubtreePostorder(OriginalIds, RootId);
    computeLeftMostDescendants();
    computeKeyRoots();
  }

  int getSizeS() const { return OriginalIds.size(); }
  NodeId getOriginalId(SNodeId Id) const {
    assert(Id > 0 && Id <= getSizeS());
    return OriginalIds[Id - 1];
  }
  const Node &getNodeS(SNodeId Id) const {
    return Tree.getNode(getOriginalId(Id));
  }
  const std::string labelS(SNodeId Id) const {
    return Tree.label(getOriginalId(Id));
  }
  SNodeId getLeftMostDescendant(SNodeId Id) const {
    assert(Id >= 0 && Id < getSizeS());
    return LeftMostDescendants[Id];
  }

private:
  void computeLeftMostDescendants() {
    LeftMostDescendants.resize(getSizeS());
    for (SNodeId I = 0; I < getSizeS(); ++I) {
      NodeId Cur = getOriginalId(I + 1);
      while (!Tree.getNode(Cur).isLeaf()) {
        Cur = Tree.getNode(Cur).Children[0];
      }
      LeftMostDescendants[I] = (Cur - getOriginalId(1));
    }
  }
  void computeKeyRoots() {
    KeyRoots.resize(LeafCount);
    std::unordered_set<SNodeId> Visited;
    int K = LeafCount - 1;
    for (int I = getSizeS(); I > 0; --I) {
      SNodeId LeftDesc = getLeftMostDescendant(I - 1);
      if (Visited.count(LeftDesc) != 0u) {
        continue;
      }
      assert(K >= 0);
      KeyRoots[K] = I;
      Visited.insert(LeftDesc);
      --K;
    }
  }
};

// Computes an optimal mapping between two trees
class ZsMatcher {
  using Pairs = std::vector<std::pair<NodeId, NodeId>>;
  using Score = double;

private:
  Subtree S1;
  Subtree S2;
  Score **TreeDist;
  Score **ForestDist;

  // TODO Use string editing distance if applicable.
  Score getUpdateCost(NodeId Id1, NodeId Id2) {
    if (!S1.getNodeS(Id1).hasSameType(S2.getNodeS(Id2))) {
      return std::numeric_limits<Score>::max();
    }
    if (S1.labelS(Id1) == S2.labelS(Id2)) {
      return 0;
    }
    return 1;
  }

  void computeTreeDist() {
    for (NodeId Id1 : S1.KeyRoots) {
      for (NodeId Id2 : S2.KeyRoots) {
        computeForestDist(Id1, Id2);
      }
    }
  }

  void dumpForestDist() const {
    for (int I = 0; I < S1.getSizeS() + 1; ++I) {
      outs() << "[";
      for (int J = 0; J < S2.getSizeS() + 1; ++J) {
        outs() << formatv("{0}{1: 2}", (J == 0 ? "" : " "), ForestDist[I][J]);
      }
      outs() << "]\n";
    }
  }

  void computeForestDist(NodeId Id1, NodeId Id2) {
    assert(Id1 > 0 && Id2 > 0);
    NodeId LMD1 = S1.getLeftMostDescendant(Id1 - 1);
    NodeId LMD2 = S2.getLeftMostDescendant(Id2 - 1);

    ForestDist[LMD1][LMD2] = 0;
    for (NodeId D1 = LMD1 + 1; D1 <= Id1; ++D1) {
      Score DeletionCost = 1.0;
      ForestDist[D1][LMD2] = ForestDist[D1 - 1][LMD2] + DeletionCost;
      for (NodeId D2 = LMD2 + 1; D2 <= Id2; ++D2) {
        Score InsertionCost = 1;
        ForestDist[LMD1][D2] = ForestDist[LMD1][D2 - 1] + InsertionCost;
        NodeId DLMD1 = S1.getLeftMostDescendant(D1 - 1);
        NodeId DLMD2 = S2.getLeftMostDescendant(D2 - 1);
        if (DLMD1 == LMD1 && DLMD2 == LMD2) {
          Score UpdateCost = getUpdateCost(D1, D2);
          ForestDist[D1][D2] =
              std::min(std::min(ForestDist[D1 - 1][D2] + DeletionCost,
                                ForestDist[D1][D2 - 1] + InsertionCost),
                       ForestDist[D1 - 1][D2 - 1] + UpdateCost);
          TreeDist[D1][D2] = ForestDist[D1][D2];
        } else {
          ForestDist[D1][D2] =
              std::min(std::min(ForestDist[D1 - 1][D2] + DeletionCost,
                                ForestDist[D1][D2 - 1] + InsertionCost),
                       ForestDist[DLMD1][DLMD2] + TreeDist[D1][D2]);
        }
      }
    }
  }

public:
  ZsMatcher(TreeRoot &T1, TreeRoot &T2, NodeId Id1, NodeId Id2)
      : S1(T1, Id1), S2(T2, Id2) {
    TreeDist = new Score *[S1.getSizeS() + 1];
    ForestDist = new Score *[S1.getSizeS() + 1];
    for (int I = 0; I < S1.getSizeS() + 1; ++I) {
      TreeDist[I] = new Score[S2.getSizeS() + 1]();
      ForestDist[I] = new Score[S2.getSizeS() + 1]();
    }
  }

  ~ZsMatcher() {
    for (int I = 0; I < S1.getSizeS() + 1; ++I) {
      delete[] TreeDist[I];
      delete[] ForestDist[I];
    }
    delete[] TreeDist;
    delete[] ForestDist;
  }

  Pairs match() {
    Pairs Matches;

    computeTreeDist();

    bool RootNodePair = true;
    Pairs TreePairs{{S1.getSizeS(), S2.getSizeS()}};

    NodeId LastRow, LastCol, FirstRow, FirstCol, Row, Col;
    while (!TreePairs.empty()) {
      std::tie(LastRow, LastCol) = TreePairs.back();
      TreePairs.pop_back();

      if (!RootNodePair) {
        computeForestDist(LastRow, LastCol);
      }

      RootNodePair = false;

      FirstRow = S1.getLeftMostDescendant(LastRow - 1);
      FirstCol = S2.getLeftMostDescendant(LastCol - 1);

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
          NodeId LMD1 = S1.getLeftMostDescendant(Row - 1);
          NodeId LMD2 = S2.getLeftMostDescendant(Col - 1);
          if (LMD1 == S1.getLeftMostDescendant(LastRow - 1) &&
              LMD2 == S2.getLeftMostDescendant(LastCol - 1)) {
            assert(S1.getNodeS(Row).hasSameType(S2.getNodeS(Col)));
            Matches.emplace_back(S1.getOriginalId(Row), S2.getOriginalId(Col));
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
