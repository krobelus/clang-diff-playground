#!/usr/bin/env python3

import json
import sys
import html

HEADER = """
<html>
<head>
<meta charset='utf-8'/>
<style>
span.d { color: red; }
span.u { color: #cc00cc; }
span.i { color: green; }
span.m { font-weight: bold; }
span   { font-weight: normal; color: black; }
div.code {
  width: 48%;
  height: 98%;
  overflow: scroll;
  float: left;
  padding: 0 0 0.5% 0.5%;
  border: solid 2px LightGrey;
  border-radius: 5px;
}
</style>
</head>
<script type='text/javascript'>
highlightStack = []
function clearHighlight() {
  while (highlightStack.length) {
    let [l, r] = highlightStack.pop()
    document.getElementById(l).style.backgroundColor = 'white'
    document.getElementById(r).style.backgroundColor = 'white'
  }
}
function highlight(event) {
  clearHighlight()
  id = event.target['id']
  if (!event.target.attributes['tid'])
    return
  tid = event.target.attributes['tid'].value
  source = document.getElementById(id)
  target = document.getElementById(tid)
  if (!target || source.parentElement && source.parentElement.classList.contains('code'))
    return
  source.style.backgroundColor = target.style.backgroundColor = 'lightgrey'
  highlightStack.push([id, tid])
  location.href = '#' + tid
}
</script>
<body>
<div onclick='highlight(event)'>
"""

FOOTER = "</pre></div></body></html>"

SOURCES = "<pre><div id='L' class='code'>%s</div><div id='R' class='code'>%s</div>"

def debug(*s):
    print(*s, file=sys.stderr)

try:
    input = open(sys.argv[1])
except:
    input = sys.stdin
root = json.load(input)

src = root['src']
src['tag'] = 'L'
src['source'] = open(src['filename']).read()
dst = root['dst']
dst['tag'] = 'R'
dst['source'] = open(dst['filename']).read()

def html_for_node(me, other, n, offset=None):
    start = offset is None
    if start:
        offset = 0
    output = ""
    # add all characters preceding this node
    for i in range(offset, n['begin']):
        output += html.escape(me['source'][i])
    offset = max(offset, n['begin'])
    if n['type']:
        href = ''
        tid = n.get('tid', -1)
        if me['tag'] == 'L':
            match = '%d -> %d' % (n['id'], tid)
        else:
            match = '%d -> %d' % (tid, n['id'])
        hovertext = '%s\n%s' % (html.escape(n['type']), match)
        if 'value' in n:
            hovertext += '\n%s' % html.escape(n['value'])
        taggedid = me['tag'] + str(n['id'])
        taggedtid = other['tag'] + str(tid)
        output += ("<span id='%s' tid='%s' title='%s'" %
                   (taggedid, taggedtid, hovertext))
        if 'change' in n:
            output += " class='%s'" % n['change']
        output += '>'
    for c in n['children']:
        (o, offset) = html_for_node(me, other, c, offset)
        output += o
    # add all characters within this node
    for r in range(offset, n['end']):
        output += html.escape(me['source'][r])
    offset = max(offset, n['end'])
    if start:
        for r in range(offset, len(me['source'])):
            output += html.escape(me['source'][r])
        offset = max(offset, len(me['source']))
    if n['type']:
        output += "</span>"
    return (output, offset)

def html_for_tree(me, other):
    return html_for_node(me, other, me['root'])[0]

h1 = html_for_tree(src, dst)
h2 = html_for_tree(dst, src)

print(HEADER, end='')
print(SOURCES % (h1, h2), end='')
print(FOOTER, end='\n')
