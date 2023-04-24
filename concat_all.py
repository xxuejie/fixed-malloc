#!/usr/bin/env python3

HEADERS = ["c-list.h", "utils.h", "linear-malloc.h", "slab-malloc.h"]
SOURCES = ["linear-malloc.c", "slab-malloc.c"]
OUTPUT = "fixed-malloc-all.h"

o = open(OUTPUT, "w")
o.write("/*\n")
o.write(" * fixed-malloc in single header format\n")
o.write(" */\n")
o.write("#ifndef FIXED_MALLOC_ALL_H_\n")
o.write("#define FIXED_MALLOC_ALL_H_\n")
o.write("\n")

for header in HEADERS:
  with open(header, "r") as i:
    o.write("/* %s */\n" % (header))
    o.writelines(i.readlines())
    o.write("\n")

o.write("#ifndef FIXED_MALLOC_DECLARATION_ONLY\n\n")
for source in SOURCES:
  with open(source, "r") as i:
    o.write("/* %s */\n" % (source))
    for line in i:
      if line.startswith("#include \""):
        line = "/* %s */\n" % ( line.strip() )
      o.write(line)
    o.write("\n")
o.write("#endif /* FIXED_MALLOC_DECLARATION_ONLY */\n\n")

o.write("#endif /* FIXED_MALLOC_ALL_H_ */\n")
o.close()
