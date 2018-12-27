#!/usr/bin/env python
import sys

src=open(sys.argv[1],'r').read()
for i,ch in enumerate(src):
  index=ord(ch)
  if not index: continue
  sys.stdout.write("  framebuffer[%d]=0x%02x;\n"%(i,index))
