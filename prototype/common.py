import sys

# gumtree compatibility
COMPATIBLE = 0

DEBUG = 1
NEW = 1

PREORDER_IDS = 1

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
    if PREORDER_IDS:
        return '%s%s(%d)' % (t[TYPE], label, t['preorder_id'])
    else:
        return '%s%s(%d)' % (t[TYPE], label, t['id'])

def indent(t):
    return "" if t['parent'] is None else " " + indent(t['parent'])

def pretty_print(t, *, file=None):
    for n in preorder(t):
        if PREORDER_IDS:
            print('%s%02d %s:%s [%s]' %
                  (indent(n), n['preorder_id'], n[TYPE], n.get(VALUE), n.get('change', '')), file=file)
        else:
            print('%s%02d %s:%s' %
                  (indent(n), n['id'], n[TYPE], n.get(VALUE)), file=file)

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

