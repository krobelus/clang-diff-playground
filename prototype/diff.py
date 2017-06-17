#!/usr/bin/env python3

import json
from heapq import heappush, heappop
from tempfile import NamedTemporaryFile
from subprocess import Popen, PIPE
import os
from os.path import abspath, dirname, realpath
import sys

import zsmatch

DEBUG = 1
BOTTOMUP = 1
EDITSCRIPT = 1

def debug(*s):
    if DEBUG:
        print(*s)

# node attributes:
# - children: list of child trees
# - type: type code
# - typeLabel: string representation of the type code
# - pos: source pos
# - length: source length
# - [label]: (optional) actual string value (name, literals)

# additional attributes
# height
# parent
# id

def fake_tree():
    return {
        'id': -1,
        'typeLabel': '',
        'children': [],
        'parent': None,
    }

def read_tree(filename):
    if filename.endswith('.java'):
        """
        Use gumtree parse to convert the source file to a syntax tree in JSON
        format. Return the tree.
        """
        treefilename = '%s.json' % filename
        with open(treefilename, 'w') as treefile:
            Popen(['./gumtree.sh', 'parse', filename], stdout=treefile).wait()
    elif filename.endswith('.json'):
        treefilename = filename
    else:
        assert False
    t = json.load(open(treefilename))['root']
    initialize_tree(t)
    return t

def GTmappings():
    """Bidirectional dict."""
    return {}

LEFT = 1
RIGHT = 2
def add_mapping(M, tsrc, tdst):
    src = tsrc['id']
    dst = tdst['id']
    if mapping_allowed(tsrc, tdst, M):
        do_add_mapping(M, src, dst)

def do_add_mapping(M, src, dst):
    left = (LEFT, src)
    right = (RIGHT, dst)
    M[left] = right
    M[right] = left

def mhasleft(M, id1):
    """Return true if there is some mapping (id1, _)."""
    return (LEFT, id1) in M

def mhasright(M, id2):
    """Return true if there is some mapping (_, id2)."""
    return (RIGHT, id2) in M

def mleft(M, id1):
    """Return id2 if there is a mapping (id1, id2)."""
    left = (LEFT, id1)
    return M[left][1] if left in M else None

def mright(M, id2):
    """Return id1 if there is a mapping (id1, id2)."""
    right = (RIGHT, id2)
    return M[right][1] if right in M else None

def mlefts(M):
    """Return all values id1 where (id1, _) in M."""
    return [left for (p, left) in M if p == LEFT]

def mrights(M):
    """Return all values id2 where (_, id2) in M."""
    return [right for (p, right) in M if p == RIGHT]

def lhasleft(A, id1):
    """Return true if there is some mapping (id1, _) in the list."""
    for (left, right) in A:
        if left == id1:
            return right
    return None

def lhasright(A, id2):
    """Return true if there is some mapping (_, id2) in the list."""
    for (left, right) in A:
        if right == id2:
            return left
    return None

def label(t):
    """Return the full label of a node."""
    return (t['typeLabel'], t.get('label'))

def isomorphic(t1, t2):
    """Compute whether two subtrees are isomorphic."""
    c1 = t1['children']
    c2 = t2['children']
    return (label(t1) == label(t2) and
            len(c1) == len(c2) and
            all(isomorphic(c1[i], c2[i]) for i in range(len(c1))))

def isomorphic_subtrees(t1, t2):
    """Get all subordinate isomorphic subtrees of two isomorphic trees."""
    assert isomorphic(t1, t2)
    yield (t1, t2)
    c1 = t1['children']
    c2 = t2['children']
    for i in range(len(c1)):
        yield from isomorphic_subtrees(c1[i], c2[i])

def initialize_tree(t, id=0, parent=None):
    """
    Set the height attribute of each subtree.
    Set the parent of each subtree.
    Set the id of each subtree to its post-order number in the tree, starting at 0
    """
    height = 0
    def nodekey(node):
        return (node['typeLabel'], node.get('label'))
    # sorted(t['children'], key=nodekey)
    for c in t['children']:
        (id, child_height) = initialize_tree(c, id, t)
        height = max(height, child_height)
    height += 1
    t['height'] = height
    t['parent'] = parent
    t['id'] = id
    return (id + 1, height)

def sn(t):
    """Return a string representation of the node."""
    if t is None:
        return 'None'
    label = ('' if 'label' not in t else ': ' + t['label'])
    return '%s%s(%d)' % (t['typeLabel'], label, t['id'])

def print_node(t):
    print(sn(t))

def pretty_print(t, *, file=None):
    for n in preorder(t):
        print('%s%02d %s:%s' %
              (indent(n), n['id'], n['typeLabel'], n.get('label')), file=file)

def GTopen(t, l):
    """Insert all children of subtree t into l."""
    for c in t['children']:
        GTpush(c, l)

def GTdice(t1, t2, M):
    """Compute the ratio of common descendants given the set of mappings M."""
    if t1 is None and t2 is None:
        return 0
    top = len([t for t in GTs(t1) if mhasleft(M, t['id'])])
    return 2 * top / (len(GTs(t1)) + len(GTs(t2)))

def GTs(t):
    """Compute the set of the descendants of t."""
    return list(preorder(t))[1:]

def preorder(t):
    """Generate all subtrees in preorder fashion."""
    yield t
    for c in t['children']:
        yield from preorder(c)

def postorder(t):
    """Generate all subtrees in postorder fashion."""
    for c in t['children']:
        yield from postorder(c)
    yield t

def bfs(t):
    fringe = [t]
    while len(fringe) > 0:
        node = fringe.pop(0)
        yield node
        fringe += node['children']

class GTpriorityList(list):
    """Height-indexed priority list, ordered by decreasing height."""
    def __init__(self, postorder):
        super().__init__()
        self.postorder = postorder

def GTpush(t, l):
    """Add the tree t to the list."""
    heappush(l, (-t['height'], t['id']))

def GTpeekMax(l):
    """Return the maximum height of the priority list."""
    return -l[0][0] if l else 0

def GTpop(l):
    """Return a list of all nodes with height equal to GTpeekMax(l)."""
    height = GTpeekMax(l)
    if height == 0:
        return []
    nodes = []
    while GTpeekMax(l) == height and l:
        id = heappop(l)[1]
        node = l.postorder[id]
        nodes += [node]
    # def idkey(node):
    #     return node['id']
    nodes.sort(key=lambda node: node['id'])
    return nodes

def GTalgorithm1(T1, T2, minHeight):
    """Top-down matching of isomorphic subtrees."""
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))

    L1 = GTpriorityList(T1postorder)
    L2 = GTpriorityList(T2postorder)

    A = []
    M = GTmappings()

    GTpush(T1, L1)
    GTpush(T2, L2)

    while min(GTpeekMax(L1), GTpeekMax(L2)) > minHeight:
        if GTpeekMax(L1) != GTpeekMax(L2):
            if GTpeekMax(L1) > GTpeekMax(L2):
                for t in GTpop(L1):
                    GTopen(t, L1)
            else:
                for t in GTpop(L2):
                    GTopen(t, L2)
        else:
            H1 = GTpop(L1)
            H2 = GTpop(L2)
            H1xH2 = ((t1, t2) for t1 in H1 for t2 in H2)
            for (t1, t2) in H1xH2:
                if isomorphic(t1, t2):
                    for (ta, tb) in isomorphic_subtrees(t1, t2):
                        add_mapping(M, ta, tb)
            for t1 in H1:
                if not mhasleft(M, t1['id']):
                    GTopen(t1, L1)
            for t2 in H2:
                if not mhasright(M, t2['id']):
                    GTopen(t2, L2)
    assert len(A) == 0
    # def dicekey(mapping):
    #     (id1, id2) = mapping
    #     t1 = T1postorder[id1]
    #     t2 = T2postorder[id2]
    #     return GTdice(t1['parent'], t2['parent'], M)
    # # order descendingly by dice value
    # sorted(A, key=dicekey, reverse=True)
    # while len(A) > 0:
    #     (id1, id2) = A.pop(0)
    #     t1 = T1postorder[id1]
    #     t2 = T2postorder[id2]
    #     for (ta, tb) in isomorphic_subtrees(t1, t2):
    #         add_mapping(M, ta, tb)
    #     A = [(u1, u2) for (u1, u2) in A if u1 != t1]
    #     A = [(u1, u2) for (u1, u2) in A if u2 != t2]
    return M

def GTcandidate(t1, M, T):
    """
    A node c in T is a candidate for t if
    1. t and c have different labels (only type)
    2. c is unmatched
    3. t and c have some matching descendants
    We then select the candidate with the greatest GTdice()
    """
    t2 = None
    bestdice = 0
    for c in postorder(T):
        if c['typeLabel'] != t1['typeLabel']:
            continue
        if mhasright(M, c['id']):
            continue
        dice = GTdice(t1, c, M)
        if dice > bestdice:
            bestdice = dice
            t2 = c
    return t2

def tree2bracketencoding(t):
    # node = '%s:%s' % ('n', '')
    node = '%s-%s' % (t['typeLabel'], t.get('label', ''))
    children = ''.join(map(tree2bracketencoding, t['children']))
    return '{%s%s}' % (node, children)

# def APTEDmapping(t1, t2):
#     """
#     Call APTED to compute a minimal edit mapping from t1 to t2.
#     """
#     f1 = NamedTemporaryFile(mode='w')
#     f1.write(tree2bracketencoding(t1))
#     f2 = NamedTemporaryFile(mode='w')
#     f2.write(tree2bracketencoding(t2))
#     f1.flush()
#     f2.flush()
#     p = Popen(['./apted.sh', '-m', '-f', f1.name, f2.name], stdout=PIPE)
#     # p = Popen(['java', '-jar', 'RTED_v1.2.jar', '-l', '-m', '-f', f1.name, f2.name], stdout=PIPE)
#     edit_distance = float(p.stdout.readline().strip())
#     mappings = []
#     for line in p.stdout:
#         x = line.strip()
#         (src, dst) = [int(n) for n in line.strip().split(b'->')]
#         if src == 0:
#             pass # node insertion; ignore
#         elif dst == 0:
#             pass # node deletion; ignore
#         mappings += [(src, dst)]
#     return mappings

def mapping_allowed(ta, tb, M):
    a_mapsto_b = mleft(M, ta['id']) == tb['id']
    sametype = ta['typeLabel'] == tb['typeLabel']
    pa = ta['parent']
    pb = tb['parent']
    parents_sametype = ((pa is None and pb is None) or
        (pa is not None and pb is not None and
        pa['typeLabel'] == pb['typeLabel'])
    )
    # TODO is this always desirable
    either_mapped = mhasleft(M, ta['id']) or mhasright(M, tb['id'])
    if either_mapped:
        return False
    return not a_mapsto_b and sametype and parents_sametype

def add_optimal_mappings(T1, T2, M, maxSize):
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))
    if max(len(GTs(T1)), len(GTs(T2))) < maxSize:
        R = zsmatch.ZsMatcher(T1, T2).match()
        for (ida, idb) in R:
            ta = T1postorder[ida]
            tb = T2postorder[idb]
            add_mapping(M, ta, tb)
        # R = APTEDmapping(T1, T2)
        # for (ida, idb) in R:
        #     # APTED encodes nodes in post-order numbering starting
        #     # from 1
        #     ida -= 1
        #     idb -= 1
        #     ta = T1postorder[ida]
        #     tb = T2postorder[idb]
        #     add_mapping(M, ta, tb)

def GTalgorithm2(T1, T2, M, minDice, maxSize):
    """Bottom-up algorithm to complete the mappings."""
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))
    for t1 in postorder(T1):
        if t1 is T1:
            add_mapping(M, T1, T2)
            add_optimal_mappings(T1, T2, M, maxSize)
            break
        matched = mhasleft(M, t1['id'])
        matched_children = any(mhasleft(M, c['id']) for c in t1['children'])
        if not matched and matched_children:
            t2 = GTcandidate(t1, M, T2)
            if t2 is not None and GTdice(t1, t2, M) > minDice:
                # TODO
                if mapping_allowed(t1, t2, M):
                    add_mapping(M, t1, t2)
                    add_optimal_mappings(t1, t2, M, maxSize)

def indent(t):
    return "" if t['parent'] is None else " " + indent(t['parent'])

# updateValue(t, v) - set the value of t to v
# add(t, tp, i, l, v) - add t as ith child of tp with label l and value v
# delete(t) - removes the leaf node t
# move(t, tp, i) - move t to be the ith child of tp

def generate_edit_script(T1, T2, M):
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))
    actions = []
    for t2 in bfs(T2):
        if mhasright(M, t2['id']):
            t1 = T1postorder[mright(M, t2['id'])]
            if t1.get('label') != t2.get('label'):
                actions += [('Update', t1, t2)]
        else:
            p2 = t2['parent']
            pid1 = mright(M, p2['id'])
            assert pid1 is not None and "parent not mapped, don't know where to insert"
            p1 = T1postorder[pid1]
            c1 = p1['children']
            c2 = p2['children']
            for pos in range(len(c2)):
                if c2[pos] is t2:
                    break
                # if not mhasleft(M, c1[pos]):
                #     break
            if pos > len(c1):
                pos = len(c1)
            # pos = max(pos, len(c1))
            actions += [('Insert', t2, p2, pos)]
            new = dict(t2)
            new['id'] = len(T1postorder)
            new['parent'] = p1
            new['children'] = []
            T1postorder += [new]
            c1.insert(pos, new)
            do_add_mapping(M, new['id'], t2['id'])
    for t1 in postorder(T1):
        if not mhasleft(M, t1['id']):
            actions += [('Delete', t1, None)]
    return actions

def GTalgorithm(T1, T2, minHeight, minDice, maxSize):
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))
    M = GTalgorithm1(T1, T2, minHeight)
    if BOTTOMUP:
        GTalgorithm2(T1, T2, M, minDice, maxSize)
    return M

minHeight = 2
minDice = 0.2
maxSize = 100

def generate_gumtree_diff_output(f1, f2):
    T1 = read_tree(f1)
    T2 = read_tree(f2)

    # write the trees to disk
    pretty_print(T1, file=open(f1 + '.tree', 'w'))
    pretty_print(T2, file=open(f2 + '.tree', 'w'))
    # open(f1 + '.btree', 'w').write(tree2bracketencoding(T1))
    # open(f2 + '.btree', 'w').write(tree2bracketencoding(T2))

    M = GTalgorithm(T1, T2, minHeight, minDice, maxSize)
    T1postorder = list(postorder(T1))
    T2postorder = list(postorder(T2))
    output = ""
    if 1:
        for id1 in mlefts(M):
            id2 = mleft(M, id1)
            t1 = T1postorder[id1]
            t2 = T2postorder[id2]
            output += 'Match %s to %s\n' % (sn(t1), sn(t2))
    if EDITSCRIPT and BOTTOMUP:
        for a in generate_edit_script(T1, T2, M):
            (act, src) = a[0:2]
            if act == 'Update':
                output += 'Update %s to %s\n' % (sn(src), a[2]['label'])
            elif act == 'Delete':
                output += 'Delete %s\n' % sn(src)
            elif act in ('Insert', 'Move'):
                dst, pos = a[2:4]
                output += '%s %s into %s at %d\n' % (act, sn(src), sn(dst), pos)
    return output

if __name__ == '__main__':
    f1 = abspath(sys.argv[1])
    f2 = abspath(sys.argv[2])
    os.chdir(dirname(dirname(realpath(__file__))))

    print(generate_gumtree_diff_output(f1, f2), end='')
