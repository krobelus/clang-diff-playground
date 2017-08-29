GSoc Project Report: Clang based C++ diff tool
==============================================

Johannes Altmanninger


Basic functionality
-------------------

I added a clang library, `Tooling/ASTDiff`, which can be used to compute a
structural diff between two clang syntax trees. It does so by applying the
gumtree algorithm to match identical and similar nodes.

The primary client, the clang tool `clang-diff` computes the changes between
the inputs source files and emits them in the following format:

```
Insert CompoundStmt(47) into FunctionDecl: m(void ())(46) at 0
Move CompoundStmt(48) into CompoundStmt(47) at 0
Move IntegerLiteral: 1(58) into BinaryOperator: +(57) at 0
Update and Move IntegerLiteral: 7(59) into BinaryOperator: +(57) at 1
```

The output contains insertions, deletions, updates and moves of different
nodes, identified by their preorder index. In general, the matching is quite
accurate, especially for small changes. However, it is not perfect and
sometimes fails to see a equivalence that is obvious to a human.

Additionally, there are also "unrelated" changes in the output, for example,
when changing the type of a symbol, every occurrence of that symbol is marked
changed. For visualisation tools that want to avoid this, it will be
necessary to work around this.

For comparing nodes, we hash the data that is contained in and referenced by
them. Similar functions are required at other places within Clang.
As of right now, the data collection for each node is done with a custom
collection class. However, the new data collection class (that's currently
    undergoing review) will reuse the recently added data collectors that are
used by the clone detection analyzer.

HTML side-by-side diff
----------------------

I have developed a HTML-based diff visualization system that shows a
side-by-side difference of the two ASTs.  It highlights additions in green,
deletions in red and updates in a shade of pink.  Furthermore, nodes that
have been moved are printed in bold.

So far I've used it mainly to debug the matching algorithm, but it can also
be used to quickly see what has changed in a patch, for example.

I've attached a couple of examples, visualizing commits to the LLVM source
in `diffs.zip`. Feel free to check them out!

You can click parts of the source that are matched to highlight and scroll to
the equivalent in the other file.
I've added some keybindings to make it easier to move through the changes.
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
implemented deletions in https://reviews.llvm.org/D37005

It works like this: There are three inputs: a source, a destination, and
a target file. The difference between source and destination is the
set of edit actions that will be applied to target.
As long as source and target are sufficiently similar, this should
almost always work.

Once the patching functionality is fully implemented, it could also be used for
a nice pattern-based refactoring engine.

Missing features
----------------
There are some features that are currently missing from clang-diff

Comparison across multiple files (headers and other included files, not just
the main file of a translation unit) - while enabling this would be
trivial, I didn't have time to implement a useful output for this.
Additionally, the comparison can run slower when including many headers.

Nodes that are a result of macro expansion were previously ignored (not matched
to any other node). There is a patch to treat macros as nodes. There the
nodes are compared just by their textual value. However, a single macro
invocation can result in multiple such macro nodes, ideally it should
be the other way round. So this patch is a bit hacky, a proper solution
requires some love.

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

Because of the complexity of the language, writing tooling for C++ is tedious
even though we have Clang. There are lots of corner cases and some language
constructs I had never used before. Additionally, it was not easy to find my
way around in this sizeable code base. I had to get used to some new tools and
workflows.

Patches
-------
Here is a list of patches that I have submitted for review.  Those up to
abed2aa96afa09c2bdec84c4c03c4d8f97fe7c0a have already been committed.
Additionally, there are some patches that are not yet published, in the
directory `unpublished_patches`. I plan to post most of them for review once
they are ready.


Patches currently being reviewed:
```
    https://reviews.llvm.org/D37201
    https://reviews.llvm.org/D37005
    https://reviews.llvm.org/D37004
    https://reviews.llvm.org/D37003
    https://reviews.llvm.org/D37001
    https://reviews.llvm.org/D36999
    https://reviews.llvm.org/D36997
    https://reviews.llvm.org/D36688
    https://reviews.llvm.org/D36687
    https://reviews.llvm.org/D36686
    https://reviews.llvm.org/D37200
    https://reviews.llvm.org/D36998
```
Patches that are already commited:
```
https://reviews.llvm.org/D37002 r311865
https://reviews.llvm.org/D37000 r311770
https://reviews.llvm.org/D37072 r311575
https://reviews.llvm.org/D36685 r311570
https://reviews.llvm.org/D36664 r311569
https://reviews.llvm.org/D36681 r311433
https://reviews.llvm.org/D36186 r311292
https://reviews.llvm.org/D36185 r311284
https://reviews.llvm.org/D36184 r311280
https://reviews.llvm.org/D36183 r311251
https://reviews.llvm.org/D36182 r311241
https://reviews.llvm.org/D36181 r311237
https://reviews.llvm.org/D36180 r311232
https://reviews.llvm.org/D36179 r311200
https://reviews.llvm.org/D36178 r311199
https://reviews.llvm.org/D36177 r311173
https://reviews.llvm.org/D36176 r311172
r311170
r309737
https://reviews.llvm.org/D34329 r308731
```
