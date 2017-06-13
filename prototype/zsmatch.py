import collections
import numpy as np
import math

def updateCost(src, dst):
    if src['typeLabel'] != dst['typeLabel']:
        return math.inf
    if not src.get('label') or not dst.get('label'):
        return 1
    return 1 - int(src['label'] == dst['label'])

def gettype(t):
    return t['typeLabel']

def label(t):
    """Return the full label of a node."""
    return (t['typeLabel'], t.get('label'))

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
        self.labels = list(postorder(t)) 
        self.start = self.labels[0]['id']
        self.nodeCount = len(self.labels)
        self.leafCount = 0
        self.llds = np.zeros(self.nodeCount, int)
        self.keyroots = None

        for i in range(len(self.labels)):
            n = self.labels[i]
            assert n['id'] - self.start == i
            self.llds[i] = getFirstLeaf(n)['id'] - self.start
            if isLeaf(n):
                self.leafCount += 1

        self.keyroots = np.zeros(self.leafCount + 1, int)
        visited = np.zeros(self.nodeCount + 1, bool)
        k = self.leafCount
        for i in range(self.nodeCount, 0, -1):
            if not visited[self.llds[i - 1] + 1]:
                self.keyroots[k] = i
                visited[self.llds[i - 1] + 1] = True
                k -= 1

    # def lld(self, i):
    #     return self.llds[i - 1] + 1

    def tree(self, i):
        return self.labels[i - 1]

class ZsMatcher:
    def __init__(self, src, dst):
        self.src = ZsTree(src)
        self.dst = ZsTree(dst)
        self.treeDist = None
        self.forestDist = None
        self.store = {}

    def computeTreeDist(self):
        self.treeDist = np.zeros((self.src.nodeCount + 1, self.dst.nodeCount + 1), float)
        self.forestDist = np.zeros((self.src.nodeCount + 1, self.dst.nodeCount + 1), float)
        for i in range(1, len(self.src.keyroots)):
            for j in range(1, len(self.dst.keyroots)):
                self.computeForestDist(self.src.keyroots[i], self.dst.keyroots[j])
        return self.treeDist

    def computeForestDist(self, i, j):
        assert i and j
        self.forestDist[self.src.llds[i - 1]][self.dst.llds[j - 1]] = 0
        for di in range(self.src.llds[i - 1] + 1, i + 1):
            costDel = 1
            self.forestDist[di][self.dst.llds[j - 1]] = (
                self.forestDist[di - 1][self.dst.llds[j - 1]] + costDel)
            for dj in range(self.dst.llds[j - 1] + 1, j + 1):
                costIns = 1
                self.forestDist[self.src.llds[i - 1]][dj] = (
                    self.forestDist[self.src.llds[i - 1]][dj - 1] + costIns)
                if (self.src.llds[di - 1] == self.src.llds[i - 1]
                    and self.dst.llds[dj - 1] == self.dst.llds[j - 1]):
                    costUpd = updateCost(self.src.tree(di), self.dst.tree(dj))
                    self.forestDist[di][dj] = min(
                        self.forestDist[di - 1][dj] + costDel,
                        self.forestDist[di][dj - 1] + costIns,
                        self.forestDist[di - 1][dj - 1] + costUpd)
                    self.treeDist[di][dj] = self.forestDist[di][dj]
                else:
                    self.forestDist[di][dj] = min(
                        self.forestDist[di - 1][dj] + costDel,
                        self.forestDist[di][dj - 1] + costIns,
                        self.forestDist[self.src.llds[di - 1]][self.dst.llds[dj - 1]]
                        + self.treeDist[di][dj])

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

            firstRow = self.src.llds[lastRow - 1]
            firstCol = self.dst.llds[lastCol - 1]

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
                    if (self.src.llds[row - 1] == self.src.llds[lastRow - 1]  and
                        self.dst.llds[col - 1] == self.dst.llds[lastCol - 1]):
                        tSrc = self.src.tree(row)
                        tDst = self.dst.tree(col)
                        assert tSrc['typeLabel'] == tDst['typeLabel']
                        mappings += [(tSrc['id'] - self.src.start,
                                      tDst['id'] - self.dst.start)]
                        row -= 1
                        col -= 1
                    else:
                        treePairs += [(row, col)]
                        row = self.src.llds[row - 1]
                        col = self.dst.llds[col - 1]
        return mappings
