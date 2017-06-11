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

struct NodeCountVisitor : public RecursiveASTVisitor<NodeCountVisitor> {
  int count = 0;
  bool TraverseDecl(Decl *D) {
    if (D != nullptr) {
      count++;
      RecursiveASTVisitor<NodeCountVisitor>::TraverseDecl(D);
    }
    return true;
  }
  bool TraverseStmt(Stmt *S) {
    if (S != nullptr) {
      count++;
      RecursiveASTVisitor<NodeCountVisitor>::TraverseStmt(S);
    }
    return true;
  }
  bool TraverseType(QualType T) {
    count++;
    RecursiveASTVisitor<NodeCountVisitor>::TraverseType(T);
    return true;
  }
};

using NodeMap = std::map<DynTypedNode, NodeId>;

struct PostorderVisitor : public RecursiveASTVisitor<PostorderVisitor> {
  int id = 0, depth = 0;
  Tree &Root;
  NodeMap &PostorderIdMap;
  PostorderVisitor(Tree &Root, NodeMap &PostorderIdMap)
      : Root(Root), PostorderIdMap(PostorderIdMap) {}
  void PreTraverse() { depth++; }
  template <typename T> void PostTraverse(T *ASTNode) {
    if (ASTNode == nullptr) {
    } else {
      depth--;
      Node &N = Root.postorder[id];
      N.parent = NoNodeId;
      N.leftMostDescendant = id;
      N.depth = depth;
      auto DNode = DynTypedNode::create(*ASTNode);
      N.node = DNode;
      PostorderIdMap[DNode] = id;
      Root.maxDepth = std::max(Root.maxDepth, depth);
      id++;
    }
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

struct PreorderVisitor : public RecursiveASTVisitor<PreorderVisitor> {
  NodeId parent = -1;
  Tree &Root;
  const NodeMap &PostorderIdMap;
  PreorderVisitor(Tree &Root, const NodeMap &PostorderIdMap)
      : Root(Root), PostorderIdMap(PostorderIdMap) {}
  template <class T> NodeId PreTraverse(T *ASTNode) {
    if (ASTNode == nullptr) {
      return parent;
    }
    auto DNode = DynTypedNode::create(*ASTNode);
    const NodeId &Id = PostorderIdMap.at(DNode);
    Root.postorder[Id].parent = parent;
    if (parent != NoNodeId) {
      Node &P = Root.postorder[parent];
      P.leftMostDescendant = std::min(P.leftMostDescendant, Id);
      P.children.push_back(Id);
    }
    parent = Id;
    return Root.postorder[Id].parent;
  }
  void PostTraverse(NodeId PreviousParent) { parent = PreviousParent; }
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

static void Preprocess(Tree &Root) {
  auto *TUD = Root.ASTUnit->getASTContext().getTranslationUnitDecl();
  NodeCountVisitor NodeCounter;
  NodeCounter.TraverseDecl(TUD);
  Root.nodeCount = NodeCounter.count;
  Root.postorder = std::vector<Node>(Root.nodeCount);
  NodeMap PostorderIdMap{};
  PostorderVisitor PostorderWalker(Root, PostorderIdMap);
  PostorderWalker.TraverseDecl(TUD);
  PreorderVisitor PreorderWalker(Root, PostorderIdMap);
  PreorderWalker.TraverseDecl(TUD);
}

class ClangDiff {
private:
  tooling::ClangTool &Tool;
  Tree T1;
  Tree T2;

  bool isomorphic(NodeId Id1, NodeId Id2) const {
    const Node &N1 = T1.postorder[Id1];
    const Node &N2 = T2.postorder[Id2];
    // TODO compare T1.label(Id1)
    if (!N1.hasSameType(N2) || N1.children.size() != N2.children.size()) {
      return false;
    }
    for (size_t I = 0; I < N1.children.size(); I++) {
      if (!isomorphic(N1.children[I], N2.children[I])) {
        return false;
      }
    }
    return true;
  }

  bool MappingAllowed(const Mappings &M, NodeId Id1, NodeId Id2) const {
    const Node &N1 = T1.postorder[Id1];
    const Node &N2 = T2.postorder[Id2];
    bool AnyMapped = M.hasSrc(Id1) || M.hasDst(Id2);
    bool SameType = N1.hasSameType(N2);
    NodeId P1 = N1.parent;
    NodeId P2 = N2.parent;
    bool ParentsSameType =
        P1 == P2 || (P1 != NoNodeId && P2 != NoNodeId &&
                     T1.postorder[P1].hasSameType(T2.postorder[P2]));
    return not AnyMapped && SameType && ParentsSameType;
  }

  void AddIsomorphicSubtrees(Mappings &M, NodeId Id1, NodeId Id2) const {
    M.link(Id1, Id2);
    const Node &N1 = T1.postorder[Id1];
    const Node &N2 = T2.postorder[Id2];
    assert(isomorphic(Id1, Id2));
    assert(N1.children.size() == N2.children.size());
    for (size_t I = 0; I < N1.children.size(); I++) {
      AddIsomorphicSubtrees(M, N1.children[I], N2.children[I]);
    }
  }

  void AddOptimalMappings(Mappings &M, NodeId Id1, NodeId Id2) const {
    if (std::max(T1.NumberOfDescendants(Id1), T2.NumberOfDescendants(Id2)) <
        MaxSize) {
      ZsMatcher Matcher;
      std::vector<std::pair<NodeId, NodeId>> R = Matcher.match(T1, T2);
      for (const auto Tuple : R) {
        NodeId Id1 = Tuple.first;
        NodeId Id2 = Tuple.second;
        if (MappingAllowed(M, Id1, Id2)) {
          M.link(Id1, Id2);
        }
      }
    }
  }

  double Dice(const Mappings &M, NodeId Id1, NodeId Id2) const {
    if (Id1 == NoNodeId || Id2 == NoNodeId) {
      return 0.0;
    }
    int CommonDescendants = 0;
    const Node &N1 = T1.postorder[Id1];
    for (NodeId Id = N1.leftMostDescendant; Id < Id1; Id++) {
      CommonDescendants += static_cast<int>(M.hasSrc(Id));
    }
    return 2.0 * CommonDescendants /
           (T1.NumberOfDescendants(Id1) + T2.NumberOfDescendants(Id2));
  }

  NodeId FindCandidate(const Mappings &M, NodeId Id1) const {
    NodeId Candidate = NoNodeId;
    double BestDiceValue = 0.0;
    const Node &N1 = T1.postorder[Id1];
    for (NodeId Id2 = 0; Id2 < T2.nodeCount; Id2++) {
      const Node &N2 = T2.postorder[Id2];
      if (!N1.hasSameType(N2)) {
        continue;
      }
      if (M.hasDst(Id2)) {
        continue;
      }
      double DiceValue = Dice(M, Id1, Id2);
      if (DiceValue > BestDiceValue) {
        BestDiceValue = DiceValue;
        Candidate = Id2;
      }
    }
    return Candidate;
  }

  void BottomUpSubtreeMatching(Mappings &M) const {
    // FIXME
    return;
    for (NodeId Id1 = 0; Id1 < T1.nodeCount; Id1++) {
      if (Id1 == T1.root()) {
        M.link(T1.root(), T2.root());
        AddOptimalMappings(M, T1.root(), T2.root());
      } else {
        const Node &N1 = T1.postorder[Id1];
        bool Matched = M.hasSrc(Id1);
        bool MatchedChildren =
            std::any_of(N1.children.begin(), N1.children.end(),
                        [=](NodeId Child) { return M.hasSrc(Child); });
        if (!Matched && MatchedChildren) {
          NodeId Id2 = FindCandidate(M, Id1);
          if (Id2 != NoNodeId && Dice(M, Id1, Id2) > MinDice) {
            if (MappingAllowed(M, Id1, Id2)) {
              M.link(Id1, Id2);
              AddOptimalMappings(M, Id1, Id2);
            }
          }
        }
      }
    }
  }

  Mappings TopDownSubtreeMatching() const {
    PriorityList L1(T1);
    PriorityList L2(T2);

    MappingList A;
    Mappings M(T1, T2);

    L1.push(T1.root());
    L2.push(T2.root());

    int Max1, Max2;
    while (std::min(Max1 = L1.peekMax(), Max2 = L2.peekMax()) > MinHeight) {
      if (Max1 != Max2) {
        if (Max1 > Max2) {
          for (NodeId Id : L1.pop()) {
            L1.open(Id);
          }
        } else {
          for (NodeId Id : L2.pop()) {
            L2.open(Id);
          }
        }
      } else {
        std::vector<NodeId> H1, H2;
        H1 = L1.pop();
        H2 = L2.pop();
        for (NodeId Id1 : H1) {
          /* Node &N1 = T1.postorder[Id1]; */
          for (NodeId Id2 : H2) {
            /* Node &N2 = T2.postorder[Id2]; */
            if (isomorphic(Id1, Id2)) {
              // TODO
              {
                if (MappingAllowed(M, Id1, Id2)) {
                  AddIsomorphicSubtrees(M, Id1, Id2);
                }
              }
            }
          }
        }
        for (NodeId Id1 : H1) {
          if (M.hasSrc(Id1) && !A.hasSrc(Id1)) {
            L1.open(Id1);
          }
        }
        for (NodeId Id2 : H2) {
          if (M.hasDst(Id2) && !A.hasDst(Id2)) {
            L2.open(Id2);
          }
        }
      }
    }
    // TODO sort, add best mappings from A to M..
    return M;
  }

  void Match() const {
    Mappings M = TopDownSubtreeMatching();
    BottomUpSubtreeMatching(M);
  }

public:
  int MinHeight = 2;
  double MinDice = 0.2;
  int MaxSize = 100;

  ClangDiff(tooling::ClangTool &Tool) : Tool(Tool) {}

  bool runDiff(const StringRef File1, const StringRef File2) {
    std::vector<std::unique_ptr<ASTUnit>> ASTs;
    Tool.buildASTs(ASTs);
    if (ASTs.size() != 2) {
      llvm::errs() << "*** error: failed to build ASTs\n";
      return false;
    }
    T1.ASTUnit = std::move(ASTs[0]);
    T2.ASTUnit = std::move(ASTs[1]);
    Preprocess(T1);
    Preprocess(T2);
    Match();
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
    llvm::errs() << "exactly two filenames required\n";
    return 1;
  }

  tooling::ClangTool Tool(Options.getCompilations(), Files);
  clang::diff::ClangDiff ClangDiff(Tool);

  bool Success = ClangDiff.runDiff(Files[0], Files[1]);
  return Success ? 0 : 1;
}
