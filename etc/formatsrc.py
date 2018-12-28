#!/usr/bin/env python
import sys,os,stat

def process_text(src):
  #return src.replace("\t","  ").replace("\r","")
  return src.replace(" if("," if (").replace(" for("," for (").replace(" switch("," switch (").replace(" while("," while (").replace(" \n","\n")

def process_regular(path):
  ext=path.split('.')[-1].lower()
  if not (ext in ('c','h','m')): return
  src=open(path,'r').read()
  dst=process_text(src)
  if src==dst: return
  sys.stdout.write("%s: modified\n"%(path,))
  open(path,'w').write(dst)

def process(path):
  st=os.stat(path)
  if stat.S_ISDIR(st.st_mode):
    for base in os.listdir(path):
      process(os.path.join(path,base))
  else:
    process_regular(path)

process("src")
