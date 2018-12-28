#!/usr/bin/env python
import sys,os,stat,re

wordpattern=re.compile("\\w+")

def replace_word(src):
  if src=="int8": return "int8_t"
  if src=="uint8": return "uint8_t"
  if src=="int16": return "int16_t"
  if src=="uint16": return "uint16_t"
  if src=="int32": return "int32_t"
  if src=="uint32": return "uint32_t"
  if src=="int64": return "int64_t"
  if src=="uint64": return "uint64_t"
  return src

def process_text(src):

  #return src.replace("\t","  ").replace("\r","")

  #return src.replace(" if("," if (").replace(" for("," for (").replace(" switch("," switch (").replace(" while("," while (").replace(" \n","\n")

  dst=""
  srcp=0
  while srcp<len(src):
    match=re.search(wordpattern,src[srcp:])
    if match is None:
      dst+=src[srcp:]
      break
    startp,stopp=match.span()
    startp+=srcp
    stopp+=srcp
    dst+=src[srcp:startp]
    word=src[startp:stopp]
    replacement=replace_word(word)
    dst+=replacement
    srcp=stopp
  return dst

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
