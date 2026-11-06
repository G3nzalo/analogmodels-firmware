#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes src/param_names.h, the parameter names of Core/Inc/Parameters.h for
the render scripts. usage: gen_param_names.py [Parameters.h] [param_names.h]"""
import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "../../Core/Inc/Parameters.h")
dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "src/param_names.h")
text = open(src, encoding="utf-8").read()
text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
text = re.sub(r"//[^\n]*", "", text)
body = text[text.index("enum ParamEnums"):]
body = body[body.index("{") + 1:body.index("};")]
names = []
for item in body.split(","):
    m = re.match(r"\s*([A-Z_][A-Z0-9_]*)", item)
    if m and (m.group(1).startswith("PAR_") or m.group(1) == "NUM_PARAMS"):
        names.append(m.group(1))
with open(dst, "w") as f:
    f.write("/* generated from Parameters.h */\n")
    f.write("static const struct { const char* name; int idx; } host_paramNames[] = {\n")
    for n in names:
        f.write('  {"%s", %s},\n' % (n, n))
    f.write("  {0, 0}\n};\n")
print("%d names" % len(names))
