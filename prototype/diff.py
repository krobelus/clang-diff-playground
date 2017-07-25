#!/usr/bin/env python3

import json
from tempfile import NamedTemporaryFile
from subprocess import Popen, PIPE
import os
from os.path import abspath, dirname, realpath
import sys
import math
import cProfile

from common import *

from out import gumtreeout
from mapping import mapping
from zsmatch import zhang_shasha_matcher

def compare_trees(t1, t2, only_isomorphicism, D):
    c1 = t1['children']
    c2 = t2['children']
    if D:
        debug(sn(t1))
        debug(t1[TYPE] == t2[TYPE],
              len(c1) == len(c2),
              (only_isomorphicism or t1.get(VALUE) == t2.get(VALUE)))
    return (t1[TYPE] == t2[TYPE] and
            len(c1) == len(c2) and
            (only_isomorphicism or t1.get(VALUE) == t2.get(VALUE)) and
            all(compare_trees(c1[i], c2[i], only_isomorphicism, D) for i in range(len(c1))))

def identical(t1, t2, D=False):
    return compare_trees(t1, t2, False, D)

def same_ident(t1, t2):
    i = 'identifier'
    qi = 'qualified_identifier'
    if qi in t1 and qi in t2 and t1[qi] == t2[qi]:
        return 1
    if i in t1 and i in t2 and t1[i] == t2[i]:
        return 0.5
    return 0

def similarity(T1, T2, t1, t2, M):
    assert t1 and t2
    assert t1[TYPE] == t2[TYPE]
    D = dpred(t1)
    nodesim = ( 0.5 * same_parents(t1, t2, M)
              + 0.5 * (t1.get(VALUE) == t2.get(VALUE))
              + 1.0 * same_ident(t1, t2))
    return minSimilarity * nodesim + jaccard_similarity(T1, T2, t1, t2, M)

def number_of_descendants(t):
    return t['rmd'] - t['preorder_id'] + 1;

def jaccard_similarity(T1, T2, t1, t2, M):
    num = number_of_common_descendants(T1, T2, t1, t2, M)
    den = number_of_descendants(t1) + number_of_descendants(t2) - num
    return num / den

def siblings_similarity(T1, T2, t1, t2, M):
    return (100 * jaccard_similarity(T1, T2, t1['parent'], t2['parent'], M)
            + 10 * pos_in_parent_similarity(t1, t2)
            + numbering_similarity(t1, t2, M)
            )

def pos_in_parent_similarity(t1, t2):
    pos1 = findpos(t1)
    pos2 = findpos(t2)
    maxpos1 = 1 if t1['parent'] is None else len(t1['parent']['children'])
    maxpos2 = 1 if t2['parent'] is None else len(t2['parent']['children'])
    maxposdiff = max(maxpos1, maxpos2)
    return 1 - abs(pos1 - pos2) / maxposdiff

def numbering_similarity(t1, t2, M):
    return 1 - abs(t1['id'] - t2['id']) / M.maxsize

def number_of_common_descendants(T1, T2, t1, t2, M):
    assert t1 and t2
    descendants = preorder(t1)
    D = dpred(t1)
    # next(descendants)
    if D and 0:
        debug([(sn(t), M.get_dst(t['id'])) for t in preorder(t1) if M.has_src(t['id'])
               and is_descendant_of(T2.npostorder[M.get_dst(t['id'])], t2)])
    return len([t for t in descendants if M.has_src(t['id'])
             and is_descendant_of(T2.npostorder[M.get_dst(t['id'])], t2)])

def number_of_distinct_descendants(T1, T2, t1, t2, M):
    return (
        len([t for t in GTs(t1) if M.has_src(t['id'])
             and not is_descendant_of(T2.npostorder[M.get_dst(t['id'])], t2)])
        +
        len([t for t in GTs(t2) if M.has_dst(t['id'])
             and not is_descendant_of(T1.npostorder[M.get_src(t['id'])], t1)])
    )

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
    nodes.sort(key=lambda node: node['preorder_id'])
    return nodes

def add_identical(M, H1, H2):
    H1xH2 = ((t1, t2) for t1 in H1 for t2 in H2)
    for (t1, t2) in H1xH2:
        if identical(t1, t2):
            M.add(t1, t2)

def top_down(T1, T2, minHeight):
    L1 = GTpriorityList(T1.npostorder)
    L2 = GTpriorityList(T2.npostorder)

    M = mapping()
    M.maxsize = max(len(T1.npostorder), len(T1.npostorder))

    # TODO
    # H1 = (t1 for t1 in T1.root['children'] if t1['height'] <= minHeight)
    # H2 = (t2 for t2 in T2.root['children'] if t2['height'] <= minHeight)
    # add_identical(M, H1, H2)

    GTpush(T1.root, L1)
    GTpush(T2.root, L2)

    while min(GTpeekMax(L1), GTpeekMax(L2)) > minHeight:
        max1, max2 = GTpeekMax(L1), GTpeekMax(L2)
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
            add_identical(M, H1, H2)
            for t1 in H1:
                if not M.has_src(t1['id']):
                    GTopen(t1, L1)
            for t2 in H2:
                if not M.has_dst(t2['id']):
                    GTopen(t2, L2)
    UniqueMapping = mapping()
    UniqueMapping.maxsize = max(len(T1.npostorder), len(T1.npostorder))
    for id1 in M.get_srcs():
        t1 = T1.npostorder[id1]
        dsts = M.get_dsts(id1)
        if len(dsts) == 1:
            id2 = dsts[0]
            if UniqueMapping.has_src(id1) or UniqueMapping.has_dst(id2):
                continue
            UniqueMapping.add(T1.npostorder[id1], T2.npostorder[id2])
    for id1 in M.get_srcs():
        dsts = M.get_dsts(id1)
        D = dpred(t1)
        if len(dsts) > 1:
            t1 = T1.npostorder[id1]
            def similarity_key(id2):
                t1 = T1.npostorder[id1]
                t2 = T2.npostorder[id2]
                return similarity(T1, T2, t1, t2, UniqueMapping)
            if D:
                for dst in dsts:
                    t2 = T2.npostorder[dst]
                    debug(similarity(T1, T2, t1, t2, UniqueMapping), 'dst', sn(t2))
            id2 = max(dsts, key=similarity_key)
            t1 = T1.npostorder[id1]
            t2 = T2.npostorder[id2]
            # TODO
            if UniqueMapping.has_src(id1) or UniqueMapping.has_dst(id2):
                continue
            UniqueMapping.add(t1, t2)
    for id1 in UniqueMapping.get_srcs():
        id2 = UniqueMapping.get_dst(id1)
        t1 = T1.npostorder[id1]
        t2 = T2.npostorder[id2]
        assert identical(t1, t2)
        for (ta, tb) in isomorphic_subtrees(t1, t2):
            UniqueMapping.add(ta, tb)
    return UniqueMapping

def add_optimal_mapping(t1, t2, M, maxSize):
    t1postorder = list(postorder(t1))
    t2postorder = list(postorder(t2))
    if max(len(t1postorder), len(t2postorder)) < maxSize:
        R = zhang_shasha_matcher(t1, t2).match()
        for (ida, idb) in R:
            # debug(ida, idb)
            ta = t1postorder[ida]
            tb = t2postorder[idb]
            if not M.has_src(ta['id']) and not M.has_dst(tb['id']):
                M.add(ta, tb)

def GTcandidate(M, T1, T2, t1, choices):
    t2 = None
    maxsimilarity = -1
    D = dpred(t1)
    for c in choices:
        if not is_mapping_allowed(t1, c):
            continue
        sim = similarity(T1, T2, t1, c, M)
        # debug('%E' % sim, sn(t1), sn(c))
        if sim >= minSimilarity and sim > maxsimilarity:
            if M.has_dst(c['id']):
                id1 = M.get_src(c['id'])
                prevt1 = T1.npostorder[id1]
                if sim <= similarity(T1, T2, prevt1, c, M):
                    continue
            maxsimilarity = sim
            t2 = c
    return t2

def map_to_best_candidate(M, T1, T2, t1, choices):
    D = dpred(t1)
    t2 = GTcandidate(M, T1, T2, t1, choices)
    if t2 is None:
        if D:
            debug('no candidate')
        return
    if D:
        debug('mbc', sn(t1), sn(t2))
    sim = similarity(T1, T2, t1, t2, M)
    accept = sim >= minSimilarity
    if not accept:
        if D:
            debug('sim too low', sim)
        return
    assert is_mapping_allowed(t1, t2)
    assert not M.has_src(t1['id'])
    oldid1 = M.get_src(t2['id']) if M.has_dst(t2['id']) else None
    remove = False
    if oldid1 is not None:
        if D:
            debug('rm')
        oldt1 = T1.npostorder[oldid1]
        remove = sim > similarity(T1, T2, oldt1, t2, M)
    if remove:
        M.unlink(oldid1, t2['id'])
    if not M.has_dst(t2['id']):
        M.add(t1, t2)
        add_optimal_mapping(t1, t2, M, maxSize)
    if remove:
        return oldt1

def bottom_up(T1, T2, M, maxSize):
    for t1 in T1.npostorder:
        if t1 is T1.root:
            t2 = T2.root
            if not M.has_src(t1['id']) and not M.has_dst(t2['id']):
                if is_mapping_allowed(t1, t2):
                    M.add(t1, t2)
                    add_optimal_mapping(t1, t2, M, maxSize)
            break
        matched = M.has_src(t1['id'])
        matched_children = any(M.has_src(c['id']) for c in t1['children'])
        if not matched and matched_children:
            while t1 is not None:
                D = dpred(t1)
                if D:
                    debug('bottom_up', sn(t1))
                t1 = map_to_best_candidate(M, T1, T2, t1, T2.npreorder)
    if 1:
        # try to match nodes with matched parents
        for t1 in T1.npreorder:
            while t1 is not None:
                D = dpred(t1)
                p1 = t1['parent']
                if p1 is None or not M.has_src(p1['id']):
                    if D:
                        debug(sn(t1))
                        debug('np', p1['id'])
                    break
                if M.has_src(t1['id']):
                    if D:
                        debug('al mapped')
                    break
                if D:
                    debug('fix', sn(t1))
                pid2 = M.get_dst(p1['id'])
                p2 = T2.npostorder[pid2]
                t1 = map_to_best_candidate(M, T1, T2, t1, preorder(p2))

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
            t1['change'] = t2['change'] = 'm'
            t2['change'] = 'm'
        if t1.get(VALUE) != t2.get(VALUE):
            t1['change'] = t2['change'] = 'u m' if t1['change'] == 'm' else 'u'

def json_diff_node(T1, T2, M, T, node):
    if T is T1:
        if M.has_src(node['id']):
            tid = T2.npostorder[M.get_dst(node['id'])][OUTID]
        else:
            tid = -1
    else:
        if M.has_dst(node['id']):
            tid = T1.npostorder[M.get_src(node['id'])][OUTID]
        else:
            tid = -1
    return ('{"id":%d,"type":"%s","begin":%d,"end":%d,%s%s%s"children":[%s]}' %
            (node[OUTID], node[TYPE], node['begin'], node['end'],
             '"change":"%s",' % node['change'] if node['change'] else '',
             '"value":%s,' % json.dumps(node[VALUE]) if 'value' in node else '',
             '"tid":%d,' % tid if tid != -1 else '',
             ','.join(
                 json_diff_node(T1, T2, M, T, c) for c in node['children'])))

def json_diff_tree(T1, T2, M, T):
    return ('{"filename":"%s"'
            ',"root":%s}' %
            (T.filename, json_diff_node(T1, T2, M, T, T.root)))

def json_diff(T1, T2, M):
    return ('{"src":%s,"dst":%s}' %
            (json_diff_tree(T1, T2, M, T1)
             ,json_diff_tree(T1, T2, M, T2)))

minHeight = 2
minSimilarity = 0.5
maxSize = 100

def main(action, f1, f2):
    T1 = Tree(json.load(open(f1)))
    T2 = Tree(json.load(open(f2)))

    # write the trees to disk
    pretty_print(T1.root, file=open(f1 + '.tree', 'w'))
    pretty_print(T2.root, file=open(f2 + '.tree', 'w'))

    M = top_down(T1, T2, minHeight)
    bottom_up(T1, T2, M, maxSize)
    compute_changes(T1, T2, M)

    if action == 'profile':
        return
    if action == 'html':
        p = Popen(['./prototype/view.py'], stdin=PIPE)
        p.communicate(input=bytes(json_diff(T1, T2, M), 'utf8'))
        return
    if action == 'jsondiff':
        print(json_diff(T1, T2, M), end='')
        return
    assert action == 'diff'
    print(gumtreeout(T1, T2, M), end='')

if __name__ == '__main__':
    i = 1
    action = 'diff'
    if len(sys.argv) > 3:
        action = sys.argv[1]
        i = 2
    f1 = abspath(sys.argv[i])
    f2 = abspath(sys.argv[i + 1])
    os.chdir(dirname(dirname(realpath(__file__))))

    if action == 'profile':
        cProfile.run('main(action, f1, f2)')
    else:
        main(action, f1, f2)
