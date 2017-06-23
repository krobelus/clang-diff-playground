import sys

DEBUG = 1
BOTTOMUP = 1
EDITSCRIPT = 1

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

# TYPE = 'typeLabel'
# VALUE = 'label'

TYPE = 'type'
VALUE = 'value'

def debug(*s):
    if DEBUG:
        print(*s, file=sys.stderr)
