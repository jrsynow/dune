# -*- coding: utf-8 -*-
############################################################################
# Copyright (C) 2007-2013 Laboratório de Sistemas e Tecnologia Subaquática #
# Departamento de Engenharia Electrotécnica e de Computadores              #
# Rua Dr. Roberto Frias, 4200-465 Porto, Portugal                          #
############################################################################
# Author: Ricardo Martins                                                  #
############################################################################

import os.path

# Sanitize the name of enumerations.
def get_enum_name(name):
    return name.replace(' ', '') + 'Enum'

# Sanitize the name of bitfields.
def get_bfield_name(name):
    return name.replace(' ', '') + 'Bits'

# Retrieve the name of a field.
def get_name(node):
    return node.get('abbrev').lower()

# Convert text to C++ comment.
def comment(text, dox = True, nl = '\n'):
    c = ''
    if dox:
        c = '!'
    return '//' + c + ' ' + text + '.' + nl

# Extract copyright from this script and convert it to a format
# suitable to be used in C++ files.
def get_cxx_copyright():
    fd = open(os.path.abspath(__file__).replace('.pyc', '.py'))
    result = []
    header = False
    for line in fd.readlines():
        line = line.strip()
        if len(line) == 0:
            break
        if line.startswith('##') and not header:
            header = True
            result.append(line)
        elif header:
            result.append(line)

    result = [l[1:] for l in result]
    result = ['//' + l for l in result]
    result = [l.replace('#', '*') for l in result]
    result.append('// Automatically generated.'.ljust(len(result[0]) - 1) + '*')
    result.append(result[0])
    return '\n'.join(result) + '\n'

# Cleanup and indent source file.
def beautify(text):
    indent = 0
    blank = False
    list0 = []

    # Remove extra empty lines and indent.
    lines = text.splitlines()
    for line in lines:
        strip = line.strip()
        if len(strip) == 0:
            if blank:
                continue
            else:
                blank = True
                list0.append('')
                continue
        else:
            blank = False

        if strip == '};' and len(list0[-1]) == 0:
            list0.pop()

        if strip == '{':
            list0.append(' ' * indent + strip)
            indent += 2
        elif strip == '}' or strip == '};':
            indent -=2
            list0.append(' ' * indent + strip)
        elif strip == 'public:' or strip == 'protected:' or strip == 'public:':
            list0.append(' ' * (indent - 2) + strip)
        else:
            list0.append(' ' * indent + strip)

    # Remove empty lines between blocks.
    list1 = []
    for line in list0:
        strip = line.strip()
        if (strip == '}' or strip == '};') and len(list1[-1]) == 0:
            list1.pop()
        list1.append(line)

    # Remove extra empty lines at EOF.
    while len(list1[-1]) == 0:
        list1.pop()

    return '\n'.join(list1) + '\n'

# Convert camel case abbrev to proper variable name.
def abbrev_to_var(abbrev, prefix = ''):
    name = ''
    for c in abbrev:
        if c.isupper():
            name += '_' + c
        else:
            name += c

    if prefix == '':
        return name
    else:
        return prefix + '_' + name[1:].lower()

# Convert camel case abbrev to proper macro name.
def abbrev_to_macro(abbrev, prefix = ''):
    name = ''
    for c in abbrev:
        if c.isupper():
            name += '_' + c
        else:
            name += c

    if prefix == '':
        return name
    else:
        return prefix + '_' + name[1:].upper()