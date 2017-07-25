from collections import defaultdict
from common import *

class mapping:
    def __init__(self):
        self.src2dst = defaultdict(list)
        self.dst2src = defaultdict(list)
        self.maxsize = None
    def add(self, tsrc, tdst):
        if dpred(tsrc):
            debug('adding', sn(tsrc), 'to', sn(tdst))
        src = tsrc['id']
        dst = tdst['id']
        self.link(src, dst)
    def link(self, src, dst):
        self.src2dst[src] += [dst]
        self.dst2src[dst] += [src]
    def unlink(self, src, dst):
        del self.src2dst[src]
        del self.dst2src[dst]
    def get_dsts(self, src):
        return self.src2dst[src]
    def get_srcs(self, dst=None):
        if dst is None:
            return [src for src in self.src2dst if self.src2dst[src]]
        return self.dst2src[dst]
    def get_dst(self, src):
        return self.get_dsts(src)[0]
    def get_src(self, dst):
        return self.get_srcs(dst)[0]
    def has_src(self, src):
        return bool(self.get_dsts(src))
    def has_dst(self, dst):
        return bool(self.get_srcs(dst))

LEFT = 'L'
RIGHT = 'R'

def add_mapping_ids(M, src, dst):
    left = (LEFT, src)
    right = (RIGHT, dst)
    M[left] += [dst]
    M[right] += [src]

def mhasleft(M, id1):
    """Return true if there is some mapping (id1, _)."""
    return (LEFT, id1) in M

def mhasright(M, id2):
    """Return true if there is some mapping (_, id2)."""
    return (RIGHT, id2) in M

def mleft(M, id1):
    """Return id2 if there is a mapping (id1, id2)."""
    left = (LEFT, id1)
    return M[left][0] if left in M else None

def mright(M, id2):
    """Return id1 if there is a mapping (id1, id2)."""
    right = (RIGHT, id2)
    return M[right][0] if right in M else None

def mleftl(M, id1):
    left = (LEFT, id1)
    return M[left] if left in M else None

def mrightl(M, id2):
    right = (RIGHT, id2)
    return M[right] if right in M else None

def mlefts(M):
    """Return all values id1 where (id1, _) in M."""
    return [left for (p, left) in M if p == LEFT]

def mrights(M):
    """Return all values id2 where (_, id2) in M."""
    return [right for (p, right) in M if p == RIGHT]
