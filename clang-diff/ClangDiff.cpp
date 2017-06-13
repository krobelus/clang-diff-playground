//===- ClangDiff.cpp - compare source files by AST nodes ------*- C++ -*- -===//
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

#include "ClangDiff.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace llvm;
using namespace clang;
using namespace tooling;

namespace clang {
namespace diff {

// Count the number of non-null nodes
struct NodeCountVisitor : public RecursiveASTVisitor<NodeCountVisitor> {
  int Count = 0;
  bool TraverseDecl(Decl *D) {
    if (D != nullptr) {
      ++Count;
      RecursiveASTVisitor<NodeCountVisitor>::TraverseDecl(D);
    }
    return true;
  }
  bool TraverseStmt(Stmt *S) {
    if (S != nullptr) {
      ++Count;
      RecursiveASTVisitor<NodeCountVisitor>::TraverseStmt(S);
    }
    return true;
  }
  bool TraverseType(QualType T) {
    ++Count;
    RecursiveASTVisitor<NodeCountVisitor>::TraverseType(T);
    return true;
  }
};

using NodeMap = std::map<DynTypedNode, NodeId>;

// Initializes the nodes in the tree, they are stored in postorder
// in Tree.Postorder.
//
// Computes depth of each node as well as the maximum depth of the AST.
//
// Creates a mapping from AST nodes (DynTypedNode) to postorder traversal
// number in PostorderIdMap.
struct PostorderVisitor : public RecursiveASTVisitor<PostorderVisitor> {
  int Id = 0, Depth = 0;
  TreeRoot &Root;
  NodeMap &PostorderIdMap;
  PostorderVisitor(TreeRoot &Root, NodeMap &PostorderIdMap)
      : Root(Root), PostorderIdMap(PostorderIdMap) {}
  void PreTraverse() { ++Depth; }
  template <typename T> void PostTraverse(T *ASTNode) {
    if (ASTNode == nullptr) {
      return;
    }
    --Depth;
    Node &N = Root.Postorder[Id];
    N.Parent = NoNodeId;
    N.LeftMostDescendant = Id;
    N.Depth = Depth;
    N.ASTNode = DynTypedNode::create(*ASTNode);
    PostorderIdMap[N.ASTNode] = Id;
    Root.MaxDepth = std::max(Root.MaxDepth, Depth);
    ++Id;
  }
  bool TraverseDecl(Decl *D) {
    if (D != nullptr) {
      PreTraverse();
      RecursiveASTVisitor<PostorderVisitor>::TraverseDecl(D);
      PostTraverse(D);
    }
    return true;
  }
  bool TraverseStmt(Stmt *S) {
    if (S != nullptr) {
      PreTraverse();
      RecursiveASTVisitor<PostorderVisitor>::TraverseStmt(S);
      PostTraverse(S);
    }
    return true;
  }
  bool TraverseType(QualType T) {
    PreTraverse();
    RecursiveASTVisitor<PostorderVisitor>::TraverseType(T);
    PostTraverse(static_cast<QualType *>(nullptr));
    return true;
  }
};

// Sets the parent and the children of each node.
//
// Computes the leftmost descendant of each node.
struct PreorderVisitor : public RecursiveASTVisitor<PreorderVisitor> {
  NodeId Parent = -1;
  TreeRoot &Root;
  const NodeMap &PostorderIdMap;
  PreorderVisitor(TreeRoot &Root, const NodeMap &PostorderIdMap)
      : Root(Root), PostorderIdMap(PostorderIdMap) {}
  template <class T> std::tuple<NodeId, NodeId> PreTraverse(T *ASTNode) {
    if (ASTNode == nullptr) {
      return {NoNodeId, Parent};
    }
    auto DNode = DynTypedNode::create(*ASTNode);
    const NodeId &Id = PostorderIdMap.at(DNode);
    Root.Postorder[Id].Parent = Parent;
    if (Parent != NoNodeId) {
      Node &P = Root.Postorder[Parent];
      P.LeftMostDescendant = std::min(P.LeftMostDescendant, Id);
      P.Children.push_back(Id);
    }
    Parent = Id;
    return {Id, Root.Postorder[Id].Parent};
  }
  void PostTraverse(std::tuple<NodeId, NodeId> State) {
    NodeId Id, PreviousParent;
    std::tie(Id, PreviousParent) = State;
    Parent = PreviousParent;
    if (Id != NoNodeId && Root.Postorder[Id].Children.empty()) {
      ++Root.LeafCount;
    }
  }
  bool TraverseDecl(Decl *D) {
    if (D != nullptr) {
      auto SavedState = PreTraverse(D);
      RecursiveASTVisitor<PreorderVisitor>::TraverseDecl(D);
      PostTraverse(SavedState);
    }
    return true;
  }
  bool TraverseStmt(Stmt *S) {
    if (S != nullptr) {
      auto SavedState = PreTraverse(S);
      RecursiveASTVisitor<PreorderVisitor>::TraverseStmt(S);
      PostTraverse(SavedState);
    }
    return true;
  }
  bool TraverseType(QualType T) {
    auto SavedState = PreTraverse(static_cast<QualType *>(nullptr));
    RecursiveASTVisitor<PreorderVisitor>::TraverseType(T);
    PostTraverse(SavedState);
    return true;
  }
};

/// Runs the above visitors to initialize the tree.
static void preprocess(TreeRoot &Root) {
  auto *TUD = Root.ASTUnit->getASTContext().getTranslationUnitDecl();
  NodeCountVisitor NodeCounter;
  NodeCounter.TraverseDecl(TUD);
  Root.NodeCount = NodeCounter.Count;
  Root.Postorder = std::vector<Node>(Root.NodeCount);
  NodeMap PostorderIdMap;
  PostorderVisitor PostorderWalker(Root, PostorderIdMap);
  PostorderWalker.TraverseDecl(TUD);
  PreorderVisitor PreorderWalker(Root, PostorderIdMap);
  PreorderWalker.TraverseDecl(TUD);
}

/// Computes the differences between two ASTs
class ClangDiff {
private:
  ClangTool &Tool;
  TreeRoot T1;
  TreeRoot T2;

  // Returns true if the two subtrees have the same structure
  // and equal node kinds (does not compare labels).
  bool isomorphic(NodeId Id1, NodeId Id2) const {
    const Node &N1 = T1.Postorder[Id1];
    const Node &N2 = T2.Postorder[Id2];
    if (!N1.hasSameType(N2) || N1.Children.size() != N2.Children.size() ||
        T1.label(Id1) != T2.label(Id2)) {
      return false;
    }
    for (size_t Id = 0; Id < N1.Children.size(); ++Id) {
      if (!isomorphic(N1.Children[Id], N2.Children[Id])) {
        return false;
      }
    }
    return true;
  }

  // TODO This may be too restrictive, we may want to allow multiple mappings
  // for nodes and resolve the ambiguity later.
  bool isMappingAllowed(const Mappings &M, NodeId Id1, NodeId Id2) const {
    const Node &N1 = T1.Postorder[Id1];
    const Node &N2 = T2.Postorder[Id2];
    bool AnyMapped = M.hasSrc(Id1) || M.hasDst(Id2);
    bool SameType = N1.hasSameType(N2);
    NodeId P1 = N1.Parent;
    NodeId P2 = N2.Parent;
    bool ParentsSameType =
        P1 == P2 || (P1 != NoNodeId && P2 != NoNodeId &&
                     T1.Postorder[P1].hasSameType(T2.Postorder[P2]));
    return not AnyMapped && SameType && ParentsSameType;
  }

  // Adds all corresponding subtrees of the two nodes to the mappings.
  // The two nodes must be isomorphic.
  void addIsomorphicSubTrees(Mappings &M, NodeId Id1, NodeId Id2) const {
    M.link(Id1, Id2);
    const Node &N1 = T1.Postorder[Id1];
    const Node &N2 = T2.Postorder[Id2];
    assert(isomorphic(Id1, Id2));
    assert(N1.Children.size() == N2.Children.size());
    for (size_t Id = 0; Id < N1.Children.size(); ++Id) {
      addIsomorphicSubTrees(M, N1.Children[Id], N2.Children[Id]);
    }
  }

  // Uses an optimal albeit slow algorithm to compute mappings for two
  // subtrees, but only if both have fewer nodes than MaxSize.
  void addOptimalMappings(Mappings &M, NodeId Id1, NodeId Id2) const {
    if (std::max(T1.numberOfDescendants(Id1), T2.numberOfDescendants(Id2)) <
        MaxSize) {
      ZsMatcher Matcher(T1, T2);
      std::vector<std::pair<NodeId, NodeId>> R = Matcher.match();
      for (const auto Tuple : R) {
        NodeId Id1 = Tuple.first;
        NodeId Id2 = Tuple.second;
        if (isMappingAllowed(M, Id1, Id2)) {
          M.link(Id1, Id2);
        }
      }
    }
  }

  // Computes the ratio of common descendants between the two nodes.
  // Descendants are only considered to be equal when they are mapped in M.
  double dice(const Mappings &M, NodeId Id1, NodeId Id2) const {
    if (Id1 == NoNodeId || Id2 == NoNodeId) {
      return 0.0;
    }
    int CommonDescendants = 0;
    const Node &N1 = T1.Postorder[Id1];
    for (NodeId Id = N1.LeftMostDescendant; Id < Id1; ++Id) {
      CommonDescendants += int(M.hasSrc(Id));
    }
    return 2.0 * CommonDescendants /
           (T1.numberOfDescendants(Id1) + T2.numberOfDescendants(Id2));
  }

  // Returns the node that has the highest degree of similarity.
  NodeId findCandidate(const Mappings &M, NodeId Id1) const {
    NodeId Candidate = NoNodeId;
    double BestDiceValue = 0.0;
    const Node &N1 = T1.Postorder[Id1];
    for (NodeId Id2 = 0; Id2 < T2.NodeCount; ++Id2) {
      const Node &N2 = T2.Postorder[Id2];
      if (!N1.hasSameType(N2)) {
        continue;
      }
      if (M.hasDst(Id2)) {
        continue;
      }
      double DiceValue = dice(M, Id1, Id2);
      if (DiceValue > BestDiceValue) {
        BestDiceValue = DiceValue;
        Candidate = Id2;
      }
    }
    return Candidate;
  }

  // Tries to match any yet unmapped nodes, in a bottom-up fashion.
  void matchBottomUp(Mappings &M) const {
    for (NodeId Id1 = 0; Id1 < T1.NodeCount; ++Id1) {
      if (Id1 == T1.root()) {
        M.link(T1.root(), T2.root());
        addOptimalMappings(M, T1.root(), T2.root());
        break;
      }
      assert(Id1 != NoNodeId);
      const Node &N1 = T1.Postorder[Id1];
      bool Matched = M.hasSrc(Id1);
      bool MatchedChildren =
          std::any_of(N1.Children.begin(), N1.Children.end(),
                      [&](NodeId Child) { return M.hasSrc(Child); });
      if (Matched || !MatchedChildren) {
        continue;
      }
      NodeId Id2 = findCandidate(M, Id1);
      if (Id2 == NoNodeId || dice(M, Id1, Id2) > MinDice ||
          !isMappingAllowed(M, Id1, Id2)) {
        continue;
      }
      M.link(Id1, Id2);
      addOptimalMappings(M, Id1, Id2);
    }
  }

  // Returns a set of mappings of isomorphic subtrees.
  Mappings matchTopDown() const {
    PriorityList L1(T1);
    PriorityList L2(T2);

    Mappings M(T1, T2);

    L1.push(T1.root());
    L2.push(T2.root());

    int Max1, Max2;
    while (std::min(Max1 = L1.peekMax(), Max2 = L2.peekMax()) > MinHeight) {
      if (Max1 > Max2) {
        for (NodeId Id : L1.pop()) {
          L1.open(Id);
        }
        continue;
      }
      if (Max2 > Max1) {
        for (NodeId Id : L2.pop()) {
          L2.open(Id);
        }
        continue;
      }
      std::vector<NodeId> H1, H2;
      H1 = L1.pop();
      H2 = L2.pop();
      for (NodeId Id1 : H1) {
        for (NodeId Id2 : H2) {
          if (isomorphic(Id1, Id2) && isMappingAllowed(M, Id1, Id2)) {
            addIsomorphicSubTrees(M, Id1, Id2);
          }
        }
      }
      for (NodeId Id1 : H1) {
        if (M.hasSrc(Id1)) {
          L1.open(Id1);
        }
      }
      for (NodeId Id2 : H2) {
        if (M.hasDst(Id2)) {
          L2.open(Id2);
        }
      }
    }
    return M;
  }

  enum ChangeKind { Delete, Update, Insert, Move };

  using Change = std::tuple<ChangeKind, NodeId, NodeId, size_t>;

  // Finds an edit script that is the difference between the two trees.
  std::vector<Change> computeChanges(Mappings &M) {
    std::vector<Change> Changes;
    for (NodeId Id2 = 0; Id2 < T2.NodeCount; ++Id2) {
      const Node &N2 = T2.Postorder[Id2];
      NodeId Id1 = M.getSrc(Id2);
      if (Id1 != NoNodeId) {
        const Node &N1 = T1.Postorder[Id1];
        assert(N1.hasSameType(N2));
        if (T1.label(Id1) != T2.label(Id2)) {
          Changes.emplace_back(Update, Id1, Id2, /*UNUSED Position=*/0);
        }
      } else {
        NodeId P2 = N2.Parent;
        NodeId P1 = M.getDst(P2);
        assert(P1 != NoNodeId);
        Node &Parent1 = T1.Postorder[P1];
        const Node &Parent2 = T2.Postorder[P2];
        auto &Siblings1 = Parent1.Children;
        const auto &Siblings2 = Parent2.Children;
        size_t Position;
        for (Position = 0; Position < Siblings2.size(); ++Position) {
          if (Siblings2[Position] == Id2 || Position >= Siblings1.size()) {
            break;
          }
        }
        llvm::errs() << Position << "\n";
        Changes.emplace_back(Insert, Id2, P2, Position);
        Node PatchNode = N2;
        PatchNode.Parent = P1;
        PatchNode.Children.clear();
        // TODO update Depth etc
        const NodeId PatchNodeId = T1.Postorder.size();
        T1.Postorder.push_back(PatchNode);
        // TODO this may be costly
        Siblings1.insert(Siblings1.begin() + Position, PatchNodeId);
        M.link(PatchNodeId, Id2);
        /*
        */
      }
    }
#if 0
    for (NodeId Id1 = 0; Id1 < T1.NodeCount; ++Id1) {
      NodeId Id2 = M.getDst(Id1);
      if (Id2 == NoNodeId) {
        Changes.emplace_back(Delete, Id1, Id2, /*UNUSED Position=*/0);
      }
    }
#endif
    return Changes;
  }

  // Prints an edit action.
  void dumpChange(const Change &Chg) const {
    ChangeKind Kind;
    NodeId Id1, Id2;
    size_t Position;
    std::tie(Kind, Id1, Id2, Position) = Chg;
    std::string S;
    switch (Kind) {
    case Delete:
      S = formatv("Delete {0}", T1.showNode(Id1));
      break;
    case Update:
      S = formatv("Update {0} to {1}", T1.showNode(Id1), T2.showNode(Id2));
      break;
    case Insert:
      S = formatv("Insert {0} to {1} at {2}", T1.showNode(Id1),
                  T2.showNode(Id2), Position);
      break;
    case Move:
      break;
    };
    llvm::errs() << S << "\n";
  }

  void match() {
    Mappings M = matchTopDown();
    matchBottomUp(M);
    M.dump();
    llvm::errs() << "T1\n";
    T1.dump();
    llvm::errs() << "T2\n";
    T2.dump();
    auto Changes = computeChanges(M);
    for (const auto &C : Changes) {
      dumpChange(C);
    }
  }

public:
  // FIXME 1 is probably too fine-grained
  int MinHeight = 1;
  double MinDice = 0.2;
  int MaxSize = 100;

  ClangDiff(ClangTool &Tool) : Tool(Tool) {}

  bool runDiff(const StringRef File1, const StringRef File2) {
    std::vector<std::unique_ptr<ASTUnit>> ASTs;
    Tool.buildASTs(ASTs);
    if (ASTs.size() != 2) {
      return false;
    }
    T1.ASTUnit = std::move(ASTs[0]);
    T2.ASTUnit = std::move(ASTs[1]);
    preprocess(T1);
    preprocess(T2);
    match();
    return true;
  }
};

} // namespace diff
} // namespace clang

static cl::OptionCategory ClangDiffCategory("clang-diff options");

int main(int argc, const char **argv) {
  CommonOptionsParser Options(argc, argv, ClangDiffCategory);
  ArrayRef<std::string> Files = Options.getSourcePathList();

  if (Files.size() != 2) {
    llvm::errs() << "Error: Exactly two filenames are required.\n";
    return 1;
  }

  tooling::ClangTool Tool(Options.getCompilations(), Files);
  clang::diff::ClangDiff ClangDiff(Tool);

  bool Success = ClangDiff.runDiff(Files[0], Files[1]);
  return Success ? 0 : 1;
}
