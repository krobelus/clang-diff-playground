import collections
import numpy as np
import math

from common import *

def updateCost(src, dst):
    if src[TYPE] != dst[TYPE]:
        return math.inf
    return 1 - int(src.get(VALUE) == dst.get(VALUE))

def postorder(t):
    """Generate all subtrees in postorder fashion."""
    for c in t['children']:
        yield from postorder(c)
    yield t

def isLeaf(t):
    return len(t['children']) == 0

def getFirstLeaf(t):
    current = t
    while not isLeaf(current):
        current = current['children'][0]
    return current


class ZsTree:
    def __init__(self, t):
        self.nodes = list(postorder(t)) 
        self.start = self.nodes[0]['id']
        self.nodeCount = len(self.nodes)
        self.leafCount = 0
        self.lmds = np.zeros(self.nodeCount, int)
        self.keyroots = None

        for i in range(len(self.nodes)):
            n = self.nodes[i]
            # debug(n['id'], self.start, i, getFirstLeaf(n)['id'])
            assert n['id'] - self.start == i
            self.lmds[i] = getFirstLeaf(n)['id'] - self.start
            if isLeaf(n):
                self.leafCount += 1
        # debug('lmd of 10', self.lmds[10])
        # print('lmds', self.lmds)

        self.keyroots = np.zeros(self.leafCount, int)
        visited = np.zeros(self.nodeCount + 1, bool)
        k = self.leafCount - 1
        for i in range(self.nodeCount, 0, -1):
            if not visited[self.lmds[i - 1]]:
                assert k >= 0
                self.keyroots[k] = i
                visited[self.lmds[i - 1]] = True
                k -= 1
        if 0:
            print('start', self.start)
            print('lmds', self.lmds)
            print('keyroots', self.keyroots)

    def valid(self, i):
        return i >= 0 and i < self.nodeCount

class ZsMatcher:
    def __init__(self, src, dst):
        self.src = ZsTree(src)
        self.dst = ZsTree(dst)
        self.treeDist = None
        self.forestDist = None

    def computeTreeDist(self):
        self.treeDist = np.zeros((self.src.nodeCount + 1, self.dst.nodeCount + 1), int)
        self.forestDist = np.zeros((self.src.nodeCount + 1, self.dst.nodeCount + 1), int)
        # for i in range(1, len(self.src.keyroots)):
        #     for j in range(1, len(self.dst.keyroots)):
                # self.computeForestDist(self.src.keyroots[i], self.dst.keyroots[j])
        for i in self.src.keyroots:
            for j in self.dst.keyroots:
                self.computeForestDist(i, j)

    def computeForestDist(self, i, j):
        assert i > 0 and j > 0
        lmdi = self.src.lmds[i - 1]
        lmdj = self.dst.lmds[j - 1]
        self.forestDist[lmdi][lmdj] = 0
        for di in range(lmdi + 1, i + 1):
            costDel = 1
            self.forestDist[di][lmdj] = (
                self.forestDist[di - 1][lmdj] + costDel)
            for dj in range(lmdj + 1, j + 1):
                costIns = 1
                self.forestDist[lmdi][dj] = (
                    self.forestDist[lmdi][dj - 1] + costIns)
                dlmdi = self.src.lmds[di - 1]
                dlmdj = self.dst.lmds[dj - 1]
                if (dlmdi == lmdi and dlmdj == lmdj):
                    assert self.src.valid(di - 1) and self.dst.valid(dj - 1)
                    costUpd = updateCost(self.src.nodes[di - 1], self.dst.nodes[dj - 1])
                    self.forestDist[di][dj] = min(
                        self.forestDist[di - 1][dj] + costDel,
                        self.forestDist[di][dj - 1] + costIns,
                        self.forestDist[di - 1][dj - 1] + costUpd)
                    self.treeDist[di][dj] = self.forestDist[di][dj]
                else:
                    self.forestDist[di][dj] = min(
                        self.forestDist[di - 1][dj] + costDel,
                        self.forestDist[di][dj - 1] + costIns,
                        self.forestDist[dlmdi][dlmdj] + self.treeDist[di][dj])
        if 0:
            for i in range(self.src.nodeCount + 1):
                print('[', end='')
                for j in range(self.dst.nodeCount + 1):
                    print(' %2d' % self.forestDist[i][j], end='')
                print(']')

    def match(self):
        mappings = []
        self.computeTreeDist()

        rootNodePair = True
        treePairs = []
        treePairs += [(self.src.nodeCount, self.dst.nodeCount)]

        while len(treePairs) > 0:
            (lastRow, lastCol) = treePairs.pop()

            if not rootNodePair:
                self.computeForestDist(lastRow, lastCol)

            rootNodePair = False

            firstRow = self.src.lmds[lastRow - 1]
            firstCol = self.dst.lmds[lastCol - 1]

            row = lastRow
            col = lastCol

            while row > firstRow or col > firstCol:
                if (row > firstRow and self.forestDist[row - 1][col] + 1 ==
                    self.forestDist[row][col]):
                    row -= 1
                elif (col > firstCol and self.forestDist[row][col - 1] + 1 ==
                      self.forestDist[row][col]):
                    col -= 1
                else:
                    lmdr = self.src.lmds[row - 1]
                    lmdc = self.dst.lmds[col - 1]
                    if (lmdr == self.src.lmds[lastRow - 1]  and
                        lmdc == self.dst.lmds[lastCol - 1]):
                        tSrc = self.src.nodes[row - 1]
                        tDst = self.dst.nodes[col - 1]
                        assert tSrc[TYPE] == tDst[TYPE]
                        mappings += [(tSrc['id'] - self.src.start,
                                      tDst['id'] - self.dst.start)]
                        row -= 1
                        col -= 1
                    else:
                        treePairs += [(row, col)]
                        row = lmdr
                        col = lmdc
        return mappings
