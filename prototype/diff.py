#!/usr/bin/env python3

import json
from heapq import heappush, heappop
from tempfile import NamedTemporaryFile
from subprocess import Popen, PIPE
import os
from os.path import abspath, dirname, realpath
import sys

from common import *

from out import gumtreeout
from mapping import mapping
from zsmatch import zhang_shasha_matcher

class Tree:
    def __init__(self, data):
        root = data['root']
        initialize_tree(root)
        i = 0
        for n in preorder(root):
            n['preorder_id'] = i
            n['tree'] = self
            n['change'] = ''
            n['shift'] = 0
            i += 1

        self.filename = data['filename']
        self.root = root
        self.npreorder = list(preorder(root))
        self.npostorder = list(postorder(root))

def isomorphic(t1, t2, D=0):
    """Compute whether two subtrees are isomorphic."""
    if D:
        print('D', sn(t1), sn(t2))
    c1 = t1['children']
    c2 = t2['children']
    return (t1[TYPE] == t2[TYPE] and
            t1.get(VALUE) == t2.get(VALUE) and
            len(c1) == len(c2) and
            all(isomorphic(c1[i], c2[i], D) for i in range(len(c1))))

def initialize_tree(t, id=0, parent=None):
    """
    Set the height attribute of each subtree.
    Set the parent of each subtree.
    Set the id of each subtree to its post-order number in the tree, starting at 0
    """
    height = 0
    for c in t['children']:
        (id, child_height) = initialize_tree(c, id, t)
        height = max(height, child_height)
    height += 1
    t['height'] = height
    t['parent'] = parent
    t['id'] = id
    return (id + 1, height)

def isomorphic_subtrees(t1, t2):
    """Get all subordinate isomorphic subtrees of two isomorphic trees."""
    yield (t1, t2)
    c1 = t1['children']
    c2 = t2['children']
    for i in range(len(c1)):
        yield from isomorphic_subtrees(c1[i], c2[i])

def GTopen(t, l):
    """Insert all children of subtree t into l."""
    for c in t['children']:
        GTpush(c, l)

def similarity(t1, t2, M):
    """Compute the ratio of common descendants given the set of mappings M."""
    if t1 is None and t2 is None:
        return 0
    top = len([t for t in GTs(t1) if M.has_src(t['id'])])
    return 2 * top / (len(GTs(t1)) + len(GTs(t2)))

def GTs(t):
    """Compute the set of the descendants of t."""
    return list(preorder(t))

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
    #TODO ?
    nodes.sort(key=lambda node: node['preorder_id'])
    return nodes

def top_down(T1, T2, minHeight):
    """Top-down matching of isomorphic subtrees."""
    L1 = GTpriorityList(T1.npostorder)
    L2 = GTpriorityList(T2.npostorder)

    M = mapping()

    GTpush(T1.root, L1)
    GTpush(T2.root, L2)

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
                D = 0
                if ('LLVMGetTypeKind' in t1.get(VALUE, '')
                    and 'LLVMGetTypeKind' in t2.get(VALUE, '')):
                    D = 0
                if D:
                    print(sn(t1), sn(t2))
                if isomorphic(t1, t2, D):
                    for (ta, tb) in isomorphic_subtrees(t1, t2):
                        M.add(ta, tb)
            for t1 in H1:
                if not M.has_src(t1['id']):
                    GTopen(t1, L1)
            for t2 in H2:
                if not M.has_dst(t2['id']):
                    GTopen(t2, L2)
    UniqueMapping = mapping()
    for id1 in M.get_srcs():
        ml = M.get_dsts(id1)
        if len(ml) == 1:
            M.add(T1.npostorder[id1], T2.npostorder[ml[0]])
    for id1 in M.get_srcs():
        ml = M.get_dsts(id1)
        if len(ml) > 1:
            def similarity_key(id2):
                t1 = T1.npostorder[id1]
                t2 = T2.npostorder[id2]
                return similarity(t1, t2, M)
            id2 = max(ml, key=similarity_key)
            UniqueMapping.add(T1.npostorder[id1], T2.npostorder[id2])
    return UniqueMapping

def GTcandidate(t1, M, T):
    """
    A node c in T is a candidate for t if
    1. t and c have different labels (only type must be equal)
    2. c is unmatched
    3. t and c have some matching descendants
    We then select the candidate with the greatest similarity()
    """
    t2 = None
    maxsimilarity = 0
    for c in preorder(T):
        if c[TYPE] != t1[TYPE]:
            continue
        if M.has_dst(c['id']):
            continue
        sim = similarity(t1, c, M)
        if sim > maxsimilarity:
            maxsimilarity = sim
            t2 = c
    return t2

def can_be_added_to_mapping(ta, tb, M):
    assert ta[TYPE] == tb[TYPE]
    pa = ta['parent']
    pb = tb['parent']
    # parents must have same type
    return ((pa is None and pb is None) or
            (pa is not None and pb is not None and
             pa[TYPE] == pb[TYPE])
    )

def add_optimal_mappings(t1, t2, M, maxSize):
    t1postorder = list(postorder(t1))
    t2postorder = list(postorder(t2))
    if max(len(t1postorder), len(t2postorder)) < maxSize:
        R = zhang_shasha_matcher(t1, t2).match()
        for (ida, idb) in R:
            ta = t1postorder[ida]
            tb = t2postorder[idb]
            if can_be_added_to_mapping(ta, tb, M):
                M.add(ta, tb)

def bottom_up(T1, T2, M, minDice, maxSize):
    """Bottom-up algorithm to complete the mappings."""
    for t1 in T1.npostorder:
        if t1 is T1.root:
            t2 = T2.root
            if t1[TYPE] == t2[TYPE]:
                M.add(t1, t2)
                add_optimal_mappings(t1, t2, M, maxSize)
            break
        matched = M.has_src(t1['id'])
        matched_children = any(M.has_src(c['id']) for c in t1['children'])
        if not matched and matched_children:
            t2 = GTcandidate(t1, M, T2.root)
            if t2 is not None and similarity(t1, t2, M) > minDice:
                if can_be_added_to_mapping(t1, t2, M):
                    # debug('linking node', sn(t1), sn(t2))
                    M.add(t1, t2)
                    add_optimal_mappings(t1, t2, M, maxSize)

def GTalgorithm(T1, T2, minHeight, minDice, maxSize):
    M = top_down(T1, T2, minHeight)
    bottom_up(T1, T2, M, minDice, maxSize)
    return M

# updateValue(t, v) - set the value of t to v
# add(t, tp, i, l, v) - add t as ith child of tp with label l and value v
# delete(t) - removes the leaf node t
# move(t, tp, i) - move t to be the ith child of tp

def same_parents(t1, t2, M):
    return ((t1['parent'] is None and
             t2['parent'] is None) or
            (t1['parent'] is not None and
             t2['parent'] is not None and
             M.has_src(t1['parent']['id']) and
             M.get_dst(t1['parent']['id']) == t2['parent']['id']))

def compute_changes(T1, T2, M):
    for t1 in T1.npostorder:
        if not M.has_src(t1['id']):
            t1['change'] = 'd'
            t1['shift'] -= 1
    for t2 in T2.npostorder:
        if not M.has_dst(t2['id']):
            t2['change'] = 'i'
            t2['shift'] += 1
    for t1 in bfs(T1.root):
        if M.has_src(t1['id']):
            id2 = M.get_dst(t1['id'])
            t2 = T2.npostorder[id2]
            if (not same_parents(t1, t2, M) or
                findpos(t1) + totalshift(t1) != findpos(t2) - totalshift(t2)):
                t1['shift'] -= 1
                t2['shift'] += 1
    for t2 in bfs(T2.root):
        if not M.has_dst(t2['id']):
            continue
        id1 = M.get_src(t2['id'])
        t1 = T1.npostorder[id1]
        if (findpos(t1) + totalshift(t1) != findpos(t2) - totalshift(t2)
            or not same_parents(t1, t2, M)):
            t1['change'] = 'm'
            t2['change'] = 'm'
        if t1.get(VALUE) != t2.get(VALUE):
            t1['change'] += ' u'
            t2['change'] += ' u'

def json_diff_node(T1, T2, M, T, node):
    id = node['id']
    try:
        tid = M.get_dst(id) if T is T1 else M.get_src(id)
    except:
        tid = -1
    return ('{"id":%d,"type":"%s","begin":%d,"end":%d,"change":"%s","value":%s,"tid":%d,"children":[%s]}' %
            (id, node[TYPE], node['begin'], node['end'], node['change'],
             json.dumps(node.get(VALUE, '')), tid, ','.join(
                 json_diff_node(T1, T2, M, T, c) for c in node['children'])))

def json_diff_tree(T1, T2, M, T):
    return ('{"filename":"%s"'
            ',"root":{"id":-1,"type":"","begin":0,"end":0,"change":"","value":"","tid":-1,"children":[%s]}}' %
            (T.filename, json_diff_node(T1, T2, M, T, T.root)))

def json_diff(T1, T2, M):
    return ('{"src":%s, "dst":%s}' %
            (json_diff_tree(T1, T2, M, T1)
             ,json_diff_tree(T1, T2, M, T2)))

minHeight = 2
if NEW:
    minHeight = 0
minDice = 0.5
maxSize = 100

def main(action, f1, f2):
    T1 = Tree(json.load(open(f1)))
    T2 = Tree(json.load(open(f2)))

    # write the trees to disk
    pretty_print(T1.root, file=open(f1 + '.tree', 'w'))
    pretty_print(T2.root, file=open(f2 + '.tree', 'w'))

    M = GTalgorithm(T1, T2, minHeight, minDice, maxSize)
    compute_changes(T1, T2, M)
    if action == 'jsondiff':
        return json_diff(T1, T2, M)
    assert action == 'diff'
    return gumtreeout(T1, T2, M)

if __name__ == '__main__':
    i = 1
    action = 'diff'
    if len(sys.argv) > 3:
        action = sys.argv[1]
        i = 2
    f1 = abspath(sys.argv[i])
    f2 = abspath(sys.argv[i + 1])
    os.chdir(dirname(dirname(realpath(__file__))))

    result = main(action, f1, f2)
    print(result, end='')
