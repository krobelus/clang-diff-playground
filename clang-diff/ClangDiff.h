#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DIFF_CLANGDIFF_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DIFF_CLANGDIFF_H

#include <memory>
#include <unordered_set>

#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/PriorityQueue.h"

using namespace llvm;
using namespace clang;
using namespace ast_type_traits;

namespace clang {
namespace diff {

using NodeId = int;

const NodeId NoNodeId = -1;

struct Node {
  NodeId parent, leftMostDescendant;
  int depth;
  DynTypedNode node;
  SmallVector<NodeId, 4> children{};

  ASTNodeKind getType() const { return node.getNodeKind(); }
  bool hasSameType(const Node &Other) const {
    return getType().isSame(Other.getType()) ||
           (getType().isNone() && Other.getType().isNone());
  }
  const StringRef getTypeLabel() const { return getType().asStringRef(); }
};

struct Tree {
  int nodeCount;
  int maxDepth = 0;
  std::unique_ptr<ASTUnit> ASTUnit;
  std::vector<Node> postorder;

  NodeId root() const {
    return postorder.empty() ? NoNodeId : postorder.size() - 1;
  }
  int NumberOfDescendants(NodeId Id) const {
    return Id - postorder[Id].leftMostDescendant;
  }
  std::string label(NodeId Id) const {
    const Node &N = postorder[Id];
    SourceRange Range = N.node.getSourceRange();
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
      const Node &N = postorder[Id];
      llvm::errs() << "type: " << N.getTypeLabel() << R"(, label: ")"
                   << label(Id) << "\"\n";
    }
  }
};

struct CompareDepth {
  const Tree &tree;
  CompareDepth(const Tree &tree) : tree(tree) {}
  bool operator()(NodeId id1, NodeId id2) const {
    return tree.postorder[id1].depth > tree.postorder[id2].depth;
  }
};

class PriorityList {
  const Tree &tree;
  CompareDepth compareDepth;
  std::vector<NodeId> container;
  PriorityQueue<NodeId, std::vector<NodeId>, CompareDepth> list;

public:
  PriorityList(const Tree &tree)
      : tree(tree), compareDepth(tree), list(compareDepth, container) {
    container.reserve(tree.postorder.size());
    for (NodeId Id = 0; Id < tree.nodeCount; Id++) {
      push(Id);
    }
  }

  void push(NodeId id) { list.push(id); }

  std::vector<NodeId> pop() {
    int Max = peekMax();
    std::vector<NodeId> Result;
    if (Max != 0) {
      while (peekMax() == Max) {
        Result.push_back(list.top());
        list.pop();
      }
    }
    return Result;
  }

  int peekMax() const {
    if (list.empty()) {
      return 0;
    }
    return 1 + tree.maxDepth - tree.postorder[list.top()].depth;
  }

  void open(NodeId id) {
    for (NodeId Child : tree.postorder[id].children) {
      push(Child);
    }
  }
};

class MappingList : std::vector<std::pair<NodeId, NodeId>> {
  std::unordered_set<NodeId> mappedSources{};
  std::unordered_set<NodeId> mappedDestinations{};

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
  Mappings(const Tree &T1, const Tree &T2) {
    src2dst = new NodeId[T1.nodeCount];
    dst2src = new NodeId[T2.nodeCount];
    // set everything to -1
    memset(src2dst, NoNodeId, T1.nodeCount * sizeof(*src2dst));
    memset(dst2src, NoNodeId, T2.nodeCount * sizeof(*dst2src));
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
public:
  std::vector<std::pair<NodeId, NodeId>> match(const Tree &T1, const Tree &T2) {
    return {};
  }
};

} // namespace diff
} // namespace clang

#endif
