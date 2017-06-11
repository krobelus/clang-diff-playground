Running
=======

```
  ./diff.py examples/1{a,b}.java
```

The scripts needs external tools; use `download-and-compile-tools.sh` to get
gumtree and apted. They require Gradle and java 8.


Algorithm
=========

* gumtree:
  - https://github.com/GumTreeDiff/gumtree
  - reference implementation of tree differencing
  - rather simple to implement
  - needs a tree edit distance algorithm

  * RTED/APTED:
    - http://tree-edit-distance.dbresearch.uni-salzburg.at/
    - state of the art
    - quite complicated, therefore it probably takes some effort to tweak it
    without breaking it
    - memory bound (see doc/dexa.pdf)

  * algorithm by Demaine et al.
    - http://erikdemaine.org/papers/TreeEdit_ICALP2007/paper.pdf
    - slightly older, less robust (worst case complexity occurs often)
    - supposedly simple to implement and adapt (?)
