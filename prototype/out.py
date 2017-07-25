from common import *

def gumtreeout(T1, T2, M):
    output = ""
    for t1 in T1.npreorder:
        if M.has_src(t1['id']):
            id2 = M.get_dst(t1['id'])
            t2 = T2.npostorder[id2]
            output += 'Match %s to %s\n' % (sn(t1), sn(t2))
    for a in generate_edit_script(T1, T2, M):
        (act, src) = a[0:2]
        if act == 'Update':
            output += 'Update %s to %s\n' % (sn(src), a[2][VALUE])
        elif act == 'Delete':
            output += 'Delete %s\n' % sn(src)
        elif act in ('Insert', 'Move'):
            dst, pos = a[2:4]
            output += '%s %s into %s at %d\n' % (act, sn(src), sn(dst), pos)
    return output

def generate_edit_script(T1, T2, M):
    actions = []
    for t2 in bfs(T2.root):
        if M.has_dst(t2['id']):
            t1 = T1.npostorder[M.get_src(t2['id'])]
            if t1.get(VALUE) != t2.get(VALUE):
                actions += [('Update', t1, t2)]
        else:
            p2 = t2['parent']
            pid1 = M.get_src(p2['id'])
            assert pid1 is not None and "parent not mapped, don't know where to insert"
            p1 = T1.npostorder[pid1]
            c1 = p1['children']
            c2 = p2['children']
            for pos in range(len(c2)):
                if c2[pos] is t2:
                    break
            if pos > len(c1):
                pos = len(c1)
            actions += [('Insert', t2, p2, pos)]
            new = dict(t2)
            new['id'] = len(T1.npostorder)
            new['parent'] = p1
            new['children'] = []
            T1.npostorder += [new]
            c1.insert(pos, new)
            M.add(new, t2)
    for t1 in T1.npostorder:
        if not M.has_src(t1['id']):
            if t1['parent'] is not None:
                t1['parent']['children'].pop(findpos(t1))
            actions += [('Delete', t1, None)]
    for t1 in T1.npostorder:
        if 'm' in t1['change']:
            t2 = T1.npostorder[M.get_dst(t1['id'])]
            actions += [('Move', t1, t2['parent'], findpos(t2) + 1)]
    return actions

# # def APTEDmapping(t1, t2):
# #     """
# #     Call APTED to compute a minimal edit mapping from t1 to t2.
# #     """
# #     f1 = NamedTemporaryFile(mode='w')
# #     f1.write(tree2bracketencoding(t1))
# #     f2 = NamedTemporaryFile(mode='w')
# #     f2.write(tree2bracketencoding(t2))
# #     f1.flush()
# #     f2.flush()
# #     p = Popen(['./apted.sh', '-m', '-f', f1.name, f2.name], stdout=PIPE)
# #     # p = Popen(['java', '-jar', 'RTED_v1.2.jar', '-l', '-m', '-f', f1.name, f2.name], stdout=PIPE)
# #     edit_distance = float(p.stdout.readline().strip())
# #     mappings = []
# #     for line in p.stdout:
# #         x = line.strip()
# #         (src, dst) = [int(n) for n in line.strip().split(b'->')]
# #         if src == 0:
# #             pass # node insertion; ignore
# #         elif dst == 0:
# #             pass # node deletion; ignore
# #         mappings += [(src, dst)]
# #     return mappings

# def tree2bracketencoding(t):
#     # node = '%s:%s' % ('n', '')
#     node = '%s-%s' % (t[TYPE], t.get(VALUE, ''))
#     children = ''.join(map(tree2bracketencoding, t['children']))
#     return '{%s%s}' % (node, children)

# def read_tree(filename):
#     if filename.endswith('.java'):
#         """
#         Use gumtree parse to convert the source file to a syntax tree in JSON
#         format. Return the tree.
#         """
#         treefilename = '%s.json' % filename
#         with open(treefilename, 'w') as treefile:
#             Popen(['./gumtree.sh', 'parse', filename], stdout=treefile).wait()
#     elif filename.endswith('.json'):
#         treefilename = filename
#     else:
#         assert False
#     data = json.load(open(treefilename))
#     return Tree(data)

            # if 0:
            #     if t1['parent'] is None or t2['parent'] is None:
            #         continue
            #     # pos1 = findpos(t1)
            #     # pos2 = findpos(t2)
            #     if not same_parents(t1, t2, M):
            #         p1 = t1['parent']
            #         p2 = t2['parent']
            #         # dst = T1.npostorder[M.get_src(p2['id'])]
            #         pos = findpos(t2)
            #         actions += [('Move', t1, p2, pos)]
            #         # TODO patch
            #         # TODO different positions
