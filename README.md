GSoc Project Report
===================

Johannes Altmanninger


Basic functionality
-------------------

I added a clang library, `Tooling/ASTDiff`, which can be used to compute a
structural diff between two clang syntax trees. It does so by applying the
gumtree algorithm to match identical and similar nodes.

The primary client, the clang tool `clang-diff` shows changes between the
inputs files like this:

```
Insert CompoundStmt(47) into FunctionDecl: m(void ())(46) at 0                                                                                                                                                                         
Move CompoundStmt(48) into CompoundStmt(47) at 0                                                                                                                                                                                       
Move IntegerLiteral: 1(58) into BinaryOperator: +(57) at 0                                                                                                                                                                             
Update and Move IntegerLiteral: 7(59) into BinaryOperator: +(57) at 1                                                                                                                                                                  
```

Which, admittedly, is not really useful by itself.

There are insertions, deletions, updates and moves of different nodes,
identified by their preorder index. In general, the matching is quite
accurate, especially for small changes. However, it is not perfect
and sometimes fails to see a equivalence that is obvious to a human.

Additionally, there are also "unrelated" changes in the output, for example,
when changing the type of a symbol, every occurrence of that symbol is marked
changed. For visualisation tools that want to avoid this, it will be
necessary to work around this.

For comparing nodes, we hash the data that is contained in and referenced by
them. Similar functions are required at other places within Clang. If it works
out, the DataCollection module will make it easier to do so.

HTML side-by-side diff
----------------------

To visualize the mappings, I have implemented an option to show a
side-by-side diff of the two inputs.
It is mainly useful to debug the matching, it is not really polished and slow
in the face of large files.

Find some examples of diffs (commits to the llvm source tree) in `diffs.zip`.

Clicking on some part of the source that is matched, highlights and scrolls to
the equivalent in the other file.
I added some keybindings to make it easier to move through the changes.
  j - move down to the next changed node that is not a descendant of the
      current node
  k - move up to the previous changed node, then select the outermost
      parent
  t - toggle between source code and tree style view (patch not yet published)
      the tree view shows a similar output to the -ast-dump option,
      each node is on a separate line
  x - focus current selection

Patching
--------

A promising use of the structural comparison is to create patching tools that
are smart in the sense that they do not need an exact match of the surrounding
text.

I have started working on this, but there was not enough time. So far I only
implemented deletions in e2ed05fe13d687490fa0efde2a251b689f27f6ab.

It works like this: There are three inputs: a source, a destination, and
a target file. The difference between source and destination is the
set of edit actions that will be applied to target.
As long as source and target are sufficiently similar, this should
almost always work.

Once this works, it could also be used for a nice pattern-based refactoring
engine.

Missing features
----------------
Comparison across multiple files (headers and other included files, not just
the main file of a translation unit) - while enabling this would be
trivial, I didn't have time to implement a useful output for this.
Additionally, the comparison can run slower when including many headers.

Nodes that are a result of macro expansion were previously ignored (not matched
to any other node). There is a patch to treat macros as nodes. There the
nodes are compared just by their textual value. However, a single macro
invocation can result in multiple such macro nodes, ideally it should
be the other way round. So this patch is a bit hacky, a proper solutions
requires some love (a perfect solution seems impossible).

Comments are also ignored at the moment. One could model them as independent
structure and use a LCS algorithm to match them. Alternatively, it might be
possible to attach them to adacent AST nodes. The former seems to be more solid.

Patching:

 * Insertions
 * There are probably some bugs where the the source range that is rewritten
   is too small / too large.
 * Fine grained patching (consider "remove VarDecl x from 'int x, y;").

Difficulties
------------

  * Long compilation times, especially when modifying widely used files.
  * Because of the complexity of the language, writing tooling for C++ is
    tedious even though we have Clang.

Patches
-------
Here is a list of patches that I have submitted for review.  Those up to
abed2aa96afa09c2bdec84c4c03c4d8f97fe7c0a have already been committed.
Additionally, there are some patches that are not yet published, in the
directory `unpublished_patches`. I plan to post most of them for review once
they are ready.


```
commit 996236c9044f6953a92bdaf8d2c6f1b252e9fdd6
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 20:05:48 2017 +0200

    [clang-diff] Use correct SourceRange for CXXConstructExpr
    
    Summary: This way the variable name of a declaration is not included
    
    Reviewers: arphaman
    
    Subscribers: klimek, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37201

commit e2ed05fe13d687490fa0efde2a251b689f27f6ab
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 27 21:54:54 2017 +0200

    [clang-diff] Initial implementation of patching
    
    This commit adds a function diff::patch that takes three syntax trees as
    inputs: a source, a destination and a patch target. It tries to apply
    the differences between source and destination to target.  Currently
    only deletions are (partially) supported.

    Differential Revision: https://reviews.llvm.org/D37005

commit 0bbf551448e9944b5f1dc7ee9a1d151aa618d303
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 16:27:54 2017 +0200

    [clang-diff] Test the HTML output for CXXOperatorCallExpr
    
    Summary:
    The operator is always the first child of such an expression.  If it is
    an infix or postfix operator, we want to print the LHS first. This
    commit merely tests that this works properly, the order is implemented
    by LexicallyOrderedRecursiveASTVisitor.
    
    Reviewers: arphaman
    
    Subscribers: klimek, cfe-commits, mgorny
    
    Differential Revision: https://reviews.llvm.org/D37004

commit 34a64d5c2aac9f41dee26f9f3fcfbeacc87e5efa
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 11:56:56 2017 +0200

    [clang-diff] Support templates
    
    Summary: TemplateName and TemplateArgument are recognised.
    
    Reviewers: arphaman
    
    Subscribers: klimek, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37003

commit 85962df52435798571097993feeedfb18e514531
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Mon Aug 14 08:56:21 2017 +0200

    [clang-diff] Use data collectors for node comparison
    
    Reviewers: arphaman
    
    Subscribers: klimek, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37001

commit 6b287f25bfb45cf7b3bcd5249a4b05cc175515cd
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Mon Aug 14 21:33:24 2017 +0200

    [AST] Add DeclDataCollectors.inc
    
    Summary:
    This adds some macros for collecting data from Decl nodes.
    However, it is incomplete as its not always clear which data should be collected,
    plus there are missing nodes. So this probably needs some tuning if it is useful
    enough to get merged.
    
    Reviewers: teemperor
    
    Subscribers: cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D36999

commit 09e440b24013b7fdf87a3def264e5cc477b9b219
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 4 15:53:15 2017 +0200

    [clang-diff] Honor macros
    
    Summary:
    Nodes that are expanded from some macro are given a special
    "Macro" kind instead of an ASTNodeKind. They are compared by
    their textual value including arguments, before expansion.
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits, klimek
    
    Differential Revision: https://reviews.llvm.org/D36997

commit accf0217f99691501cf9cdce6d6fbaaac650f761
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Mon Aug 14 17:51:08 2017 +0200

    [clang-diff] Fix matching for unnamed NamedDecls
    
    Summary:
    This makes Node::getIdentifier return a valid value for special
    NamedDecl nodes that do not have a name, such as ctors, dtors
    and unnamed classes / namespaces.
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits, klimek
    
    Differential Revision: https://reviews.llvm.org/D36688

commit 0f6945fdcab013013ed7bf4539280285eb126d64
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Thu Aug 3 09:19:56 2017 +0200

    [clang-diff] Match nodes with the same parent and same value
    
    Summary:
    This adds another pass to the matching algorithm that tries to match nodes with
    common parents, if they have similar values.
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits, klimek
    
    Differential Revision: https://reviews.llvm.org/D36687

commit 6de171cf27cccfc7529bde6b6a2e89700627501c
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Thu Aug 10 15:23:42 2017 +0200

    [clang-diff] Add option to compare files across git revisions
    
    Summary:
    This adds a command line option "-git-rev=<commit>".
    When it is used, only one filename is accepted.  The file in its version
    in the specified revision is compared against the current version. Note
    that this calls `git checkout` in the current directory.
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D36686

commit 708dda9fa8372d269dca0a116f4503e871c20b87
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 26 20:59:17 2017 +0200

    [AST] Traverse CXXOperatorCallExpr in LexicallyOrderedRecursiveASTVisitor
    
    Summary:
    This affects overloaded operators, which are represented by a
    CXXOperatorCallExpr whose first child is always a DeclRefExpr referring to the
    operator. For infix, postfix and call operators we want the first argument
    to be traversed before the operator.
    
    Reviewers: arphaman
    
    Subscribers: klimek, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37200

commit a6b545072c24335423beef7706fbc58903add2cd
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 25 10:43:17 2017 +0200

    [AST] Traverse templates in LexicallyOrderedRecursiveASTVisitor
    
    We need to specialize this because RecursiveASTVisitor visits template
    template parameters after the templated declaration, unlike the order in
    which they appear in the source code.

commit abed2aa96afa09c2bdec84c4c03c4d8f97fe7c0a
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 27 22:52:20 2017 +0000

    [clang-diff] Treat CXXCtorInitializer as a node
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits, klimek
    
    Differential Revision: https://reviews.llvm.org/D37002
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311865 91177308-0d34-0410-b5e6-96231b3b80d8

commit 2a74f265b5b1ae275ae0829c632b662ff63d42bc
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 25 09:49:49 2017 +0000

    [clang-diff] Remove NodeCountVisitor, NFC
    
    Subscribers: klimek, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37000
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311770 91177308-0d34-0410-b5e6-96231b3b80d8

commit 82576d34831de0b570dc4ad7a85bb26866fb882f
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 16:52:15 2017 +0000

    [clang-diff] Properly clear the selection in HTML diff
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D37072
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311575 91177308-0d34-0410-b5e6-96231b3b80d8

commit 5404f128efad6f1e92454a09a739474d59dc47e5
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 16:32:35 2017 +0000

    [clang-diff] Reformat test, NFC
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311571 91177308-0d34-0410-b5e6-96231b3b80d8

commit 78a71a1c1d9a2a0d4830f35729c6e54d7e0154ac
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 16:32:13 2017 +0000

    [clang-diff] HTML diff navigation
    
    Summary:
    This adds shortcuts j and k to jump between changes.
    It is especially useful in diffs with few changes.
    
    Reviewers: arphaman
    
    Subscribers: cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D36685
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311570 91177308-0d34-0410-b5e6-96231b3b80d8

commit 60830325117d250680532611af3c73b087ee891b
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 16:28:26 2017 +0000

    [analyzer] Make StmtDataCollector customizable
    
    Summary:
    This moves the data collection macro calls for Stmt nodes
    to lib/AST/StmtDataCollectors.inc
    
    Users can subclass ConstStmtVisitor and include StmtDataCollectors.inc
    to define visitor methods for each Stmt subclass. This makes it also
    possible to customize the visit methods as exemplified in
    lib/Analysis/CloneDetection.cpp.
    
    Move helper methods for data collection to a new module,
    AST/DataCollection.
    
    Add data collection for DeclRefExpr, MemberExpr and some literals.
    
    Reviewers: arphaman, teemperor!
    
    Subscribers: mgorny, xazax.hun, cfe-commits
    
    Differential Revision: https://reviews.llvm.org/D36664
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311569 91177308-0d34-0410-b5e6-96231b3b80d8

commit ba1ec3c54a66286fe964b2f79286e706e45e7b9e
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Wed Aug 23 10:43:26 2017 +0000

    Fix typos, remove unused private members of CommonOptionsParser, NFC
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311544 91177308-0d34-0410-b5e6-96231b3b80d8

commit ce7e9f1524037ca01fa3c5d395dcde87beb25dba
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Tue Aug 22 08:59:13 2017 +0000

    [clang-diff] Fix getRelativeName
    
    Handle the case when DeclContext is null.
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311434 91177308-0d34-0410-b5e6-96231b3b80d8

commit c690c1e9f118a8358fc7eb097f080b5b3ab6018f
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Tue Aug 22 08:56:26 2017 +0000

    [clang-diff] Use the relative name for NamedDecl
    
    Summary:
    If a node referring to a name is within a class or namespace, do not use
    the full qualified name, but strip the namespace prefix.
    
    Reviewers: arphaman, bkramer
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36681
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311433 91177308-0d34-0410-b5e6-96231b3b80d8

commit 8982819d15f4ba52ab853569c849121db58776fb
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 20:13:33 2017 +0000

    Allow thiscall attribute in test/Tooling/clang-diff-ast.cpp
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311301 91177308-0d34-0410-b5e6-96231b3b80d8

commit fbdebfae845511dae1ff852aa303205a1dee5ebf
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 16:18:43 2017 +0000

    [clang-diff] Improve and test getNodeValue
    
    Summary: Use qualified names if available.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36186
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311292 91177308-0d34-0410-b5e6-96231b3b80d8

commit c77ca07725884bbec4ba5940e731dc36f8f71777
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 12:09:07 2017 +0000

    [clang-diff] Fix similarity computation
    
    Summary:
    Add separate tests for the top-down and the bottom-up phase, as well as
    one for the optimal matching.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36185
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311284 91177308-0d34-0410-b5e6-96231b3b80d8

commit 544626fd1a0364617e75c3511addbff936ce315a
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sun Aug 20 10:22:32 2017 +0000

    [clang-diff] Filter AST nodes
    
    Summary:
    Ignore macros and implicit AST nodes, as well as anything outside of the
    main source file.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36184
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311280 91177308-0d34-0410-b5e6-96231b3b80d8

commit c778ea4b266d5fdcf3ab2d2509b19083bc43a91e
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 17:53:01 2017 +0000

    [clang-diff] Simplify mapping
    
    Summary:
    Until we find a decent heuristic on how to choose between multiple
    identical trees, there is no point in supporting multiple mappings.
    
    This also enables matching of nodes with parents of different types,
    because there are many instances where this is appropriate.  For
    example for and foreach statements; functions in the global or
    other namespaces.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36183
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311251 91177308-0d34-0410-b5e6-96231b3b80d8

commit 6f7e9aa48c4820979d278ff3ee5d597fa2e35b31
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 17:52:29 2017 +0000

    Add clang-diff to tool_patterns in test/lit.cfg
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311250 91177308-0d34-0410-b5e6-96231b3b80d8

commit 411b3e22d47e5292e3ed4dc96405d2b5f966c9b0
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 17:12:25 2017 +0000

    [clang-diff] Fix compiler warning
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311249 91177308-0d34-0410-b5e6-96231b3b80d8

commit 1e7d61fd34a2cab2fbd4a3633c19fdcd00c6dced
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 15:40:45 2017 +0000

    [clang-diff] Add HTML side-by-side diff output
    
    Reviewers: arphaman
    
    Subscribers: mgorny
    
    Differential Revision: https://reviews.llvm.org/D36182
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311241 91177308-0d34-0410-b5e6-96231b3b80d8

commit d159a31098954a88c859c6e9cc2d2600d1e3cf93
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 13:29:44 2017 +0000

    [clang-diff] Fix warning about useless comparison
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311240 91177308-0d34-0410-b5e6-96231b3b80d8

commit 710ea0b071ca5dcf2504a2e49eee1a5b315f3215
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 12:04:04 2017 +0000

    [clang-diff] Make printing of matches optional
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36181
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311237 91177308-0d34-0410-b5e6-96231b3b80d8

commit c4dadfdb220bd4a3b9c40bb931411dabbcdab013
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 10:05:24 2017 +0000

    [clang-diff] Fix test
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311233 91177308-0d34-0410-b5e6-96231b3b80d8

commit d9cea6adc496c110f6f2c0775f32d169ae78ee18
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 09:36:14 2017 +0000

    [clang-diff] Add option to dump the AST, one node per line
    
    Summary:
    This is done with -ast-dump; the JSON variant has been renamed to
    -ast-dump-json.
    
    Reviewers: arphaman
    
    Differential Revision: https://reviews.llvm.org/D36180
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311232 91177308-0d34-0410-b5e6-96231b3b80d8

commit c15e70eae870a8683089e94c1cc6e9c238a094a6
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 02:56:35 2017 +0000

    Revert "Revert "[clang-diff] Move printing of matches and changes to clang-diff""
    
    Fix build by renaming ChangeKind -> Change
    
    This reverts commit 0c78c5729f29315d7945988efd048c0cb86c07ce.
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311222 91177308-0d34-0410-b5e6-96231b3b80d8

commit 2ebc6faf999708676fa08524ffeb0df8d1988e3a
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 01:34:24 2017 +0000

    [clang-diff] Fix test for python 3
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311220 91177308-0d34-0410-b5e6-96231b3b80d8

commit 948113089e026114699a517bacf0a85987f6a5c8
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Sat Aug 19 00:57:38 2017 +0000

    Revert "Revert "[clang-diff] Move the JSON export function to clang-diff""
    
    This reverts commit eac4c13ac9ea8f12bc049e040c7b9c8a517f54e7, the
    original commit *should* not have caused the build failure.
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311216 91177308-0d34-0410-b5e6-96231b3b80d8

commit ee444f1e76dacd6593a5924b4f15bbec471010f7
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 18 21:26:34 2017 +0000

    [clang-diff] Move printing of matches and changes to clang-diff
    
    Summary:
    This also changes the output order of the changes. Now the matches are
    printed in pre-order, intertwined with insertions, updates, and moves.
    Deletions are printed afterwards.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36179
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311200 91177308-0d34-0410-b5e6-96231b3b80d8

commit 5ce0ae8c6ac9dc99e4807f2989be4c6c50975c5e
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 18 21:26:13 2017 +0000

    [clang-diff] Move the JSON export function to clang-diff
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36178
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311199 91177308-0d34-0410-b5e6-96231b3b80d8

commit 3c9f486dfd8d87757e064908ce8881a4a710ff8b
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 18 16:34:22 2017 +0000

    [clang-diff] Add commandline arguments.
    
    Summary:
    Support command line options for build path and extra arguments
    This emulates the options accepted by clang tools that use CommonOptionsParser.
    
    Add a flag for controlling the maximum size parameter for bottom up matching.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36177
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311173 91177308-0d34-0410-b5e6-96231b3b80d8

commit 7fe9c979c5ab07417d9b9424cf3d3640172bcc13
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 18 16:34:15 2017 +0000

    [clang-diff] Fix some errors and inconsistencies
    
    Fix to the computation of the rightmost descendant.
    
    Prevents root nodes from being mapped if they are already mapped.  This
    only makes a difference when we compare AST Nodes other than entire
    translation units, a feature which still has to be tested.
    
    Reviewers: arphaman
    
    Subscribers: klimek
    
    Differential Revision: https://reviews.llvm.org/D36176
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311172 91177308-0d34-0410-b5e6-96231b3b80d8

commit 84dc519667a4f06ed078f08eff2372a223a50b3e
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Fri Aug 18 16:21:08 2017 +0000

    [CommonOptionsParser] Expose ArgumentsAdjustingCompilationDatabase
    
    This is useful for tools such as clang-diff which do not use
    CommonOptionsParser due to the need for multiple compilation databases.
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@311170 91177308-0d34-0410-b5e6-96231b3b80d8

commit 6a453c4b1e3ca70fc1f319005e45e2af1f7a105c
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Tue Aug 1 20:17:46 2017 +0000

    [clang-diff] Renames, NFC
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@309738 91177308-0d34-0410-b5e6-96231b3b80d8

commit 298225638768903f20a9686991ae8fe39f3a0158
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Tue Aug 1 20:17:40 2017 +0000

    [clang-diff] Move data declarations to the public header
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@309737 91177308-0d34-0410-b5e6-96231b3b80d8

commit 5521225e7b38f6326210c808ce78ebca02d6f326
Author: Johannes Altmanninger <aclopte@gmail.com>
Date:   Thu Jul 27 15:04:44 2017 +0000

    [clang-diff] Rename, NFC
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@309276 91177308-0d34-0410-b5e6-96231b3b80d8

commit 02689b0536857e85f48e9958baff00de321323e2
Author: Alex Lorenz <arphaman@gmail.com>
Date:   Fri Jul 21 12:49:28 2017 +0000

    [clang-diff] Add initial implementation
    
    This is the first commit for the "Clang-based C/C++ diff tool" GSoC project.
    
    ASTDiff is a new library that computes a structural AST diff between two ASTs
    using the gumtree algorithm. Clang-diff is a new Clang tool that will show
    the structural code changes between different ASTs.
    
    Patch by Johannes Altmanninger!
    
    Differential Revision: https://reviews.llvm.org/D34329
    
    
    git-svn-id: https://llvm.org/svn/llvm-project/cfe/trunk@308731 91177308-0d34-0410-b5e6-96231b3b80d8
```
