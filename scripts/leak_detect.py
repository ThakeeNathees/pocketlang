##
## Copyright (c) 2020-2022 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License
##

## A quick script to detect memory leaks, from the trace report.
## To get the trace report redefine TRACE_MEMORY as 1 at the
## pk_internal.h and compile pocketlang.

raise "This script Need refactor after removing pkAllocString " + \
      "and adding pkRealloc"

import sys

def detect_leak():
  trace_log_id = "[memory trace]"
  total_bytes = 0 ## Totally allocated bytes.
  mem = dict()    ## key = address, value = bytes.

  str_alloc_id = "[alloc string]"
  strs = dict()   ## string allocations.

  trace_log_file_path = sys.argv[1]

  with open(trace_log_file_path, 'r') as f:
    for line in f.readlines():
      if line.startswith(str_alloc_id):
        line = line[len(trace_log_id) + 1:].strip()
        type, data = split_and_strip(line, ':')
        if type == "alloc":
          addr, bytes = split_and_strip(data, '=')
          bytes = int(bytes.replace(' bytes', ''))
          strs[addr] = bytes

        else: ## "dealloc"
          addr = data.strip()
          strs.pop(addr)

      elif line.startswith(trace_log_id):
        line = line[len(trace_log_id) + 1:].strip()
        type, data = split_and_strip(line, ':')
        
        addr, bytes = split_and_strip(data, '=')
        bytes = bytes.replace(' bytes', '')

        if type == "malloc":
          bytes = int(bytes)
          assert(bytes >= 0); total_bytes += bytes
          mem[addr] = bytes

        elif type == "free":
          bytes = int(bytes)
          assert(bytes <= 0); total_bytes += bytes
          mem.pop(addr)

        elif type == "realloc":
          oldp, newp = split_and_strip(addr, '->')
          olds, news = split_and_strip(bytes, '->')
          olds = int(olds); news = int(news)
          total_bytes += (news - olds)
          assert(mem[oldp] == olds)
          mem.pop(oldp)
          mem[newp] = news

  success = True
  if total_bytes != 0:
    print_err(f"Memory leak detected - {total_bytes} bytes were never freed.")
    success = False

  if len(mem) != 0:
    print_err("Memory leak detected - some addresses were never freed.")
    for addr in mem:
      print(f"  {addr} : {mem[addr]} bytes")
    success = False

  if len(strs) != 0:
    print_err("Memory leak detected - string allocation(s) were never freed.")
    for addr in strs:
      print(f"  {addr} : {strs[addr]} bytes")
    success = False

  if success:
    print("No leaks were detected.")
  else:
    sys.exit(1)


def split_and_strip(string, delim):  
  return map(lambda s: s.strip(), string.split(delim))


def print_err(msg):
  print("Error:", msg, file=sys.stderr)


if __name__ == "__main__":
  detect_leak()
