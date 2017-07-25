import sys
import math
from heapq import heappush, heappop
import re

# gumtree compatibility
COMPATIBLE = 0

#print debug info
DEBUG = 1

# PREORDER_IDS = 1
OUTID = 'preorder_id'

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

if COMPATIBLE:
    TYPE = 'typeLabel'
    VALUE = 'label'
else:
    TYPE = 'type'
    VALUE = 'value'

def debug(*s):
    if DEBUG:
        print(*s, file=sys.stderr)

def findpos(t):
    """Return the position of the node amongst its siblings."""
    p = t['parent']
    if p is None:
        return 0
    for i in range(len(p['children'])):
        if p['children'][i] is t:
            return i
    assert False

def totalshift(t):
    p = t['parent']
    if p is None:
        return 0
    shift = 0
    for i in range(len(p['children'])):
        shift += p['children'][i]['shift']
        if p['children'][i] is t:
            break
    return shift

def sn(t):
    """Return a string representation of the node."""
    if t is None:
        return 'None'
    label = ('' if VALUE not in t else ': ' + t[VALUE])
    return '%s%s(%d)' % (t[TYPE], label, t[OUTID])

def indent(t):
    return "" if t['parent'] is None else " " + indent(t['parent'])

def pretty_print(t, *, file=None):
    for n in preorder(t):
        print('%s%02d %s:%s [%s]' %
              (indent(n), n[OUTID], n[TYPE], n.get(VALUE), n.get('change', '')), file=file)

"""Generate all subtrees in preorder fashion."""
def preorder(t):
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

        self.filename = data.get('filename')
        self.root = root
        self.npreorder = list(preorder(root))
        self.npostorder = list(postorder(root))

        for n in self.npostorder:
            if n['children']:
                n['rmd'] = n['children'][-1]['rmd']
            else:
                n['rmd'] = n['preorder_id']

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

def is_descendant_of(d, p):
    return (d['preorder_id'] >= p['preorder_id']
            and d['preorder_id'] <= p['rmd'])
    # while d is not None:
    #     if d is p:
    #         return True
    #     d = d['parent']

def same_parents(t1, t2, M):
    return ((t1['parent'] is None and
             t2['parent'] is None) or
            (t1['parent'] is not None and
             t2['parent'] is not None and
             M.has_src(t1['parent']['id']) and
             M.get_dst(t1['parent']['id']) == t2['parent']['id']))

def idmatch(t1, t2):
    assert t1[TYPE] == t2[TYPE]
    # qualified
    # . because of (anonymous namespace)
    x1 = '^.[^(]+\\('
    # unqualified
    x2 = '[^(:]+\\('
    names = {'CXXMethodDecl': (x1, x2),
             'FunctionDecl': ('[a-zA-Z_][^\\s\\(]*\\(', ),
             'CXXConstructorDecl': (x1, x2)
             }
    # if t1.get(VALUE) == t2.get(VALUE):
    #     return 1
    if t1[TYPE] in names:
        score = 2
        for regex in names[t1[TYPE]]:
            p = re.compile(regex)
            def g(t):
                if not p.search(t.get(VALUE)):
                    debug(t.get(VALUE))
                return p.search(t.get(VALUE)).group()
            if g(t1) == g(t2):
                return score / 2
            score -= 1
    return 0
        # if t1.get(VALUE) != t2.get(VALUE):
        #     return True
    # return False

def is_mapping_allowed(t1, t2):
    if t1[TYPE] != t2[TYPE]:
        return False
    # if t1.get('idmatch', False) or t2.get('idmatch', False):
    #     if idmismatch(t1, t2):
    #         return False
    return True

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

def GTopen(t, l):
    """Insert all children of subtree t into l."""
    for c in t['children']:
        GTpush(c, l)

def updateCost(src, dst):
    if not is_mapping_allowed(src, dst):
        return math.inf
    l = src.get(VALUE, '')
    r = dst.get(VALUE, '')
    return l != r
    # import editdistance
    # print(editdistance.eval(l, r), file=sys.stderr)
    # return 1 - (editdistance.eval(l, r) / (1 + max(len(l), len(r))))

def dpred(t1):
    return False
    # return t1['id'] in (5220, )
    # return t1['id'] in (3816, )
    # return t1['id'] in (1507, )
    return t1[OUTID] in (3699, )
    # return t1[TYPE] == 'IfStmt'
