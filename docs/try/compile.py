#!python
## Copyright (c) 2021 Thakee Nathees
## Licensed under: MIT License

## TODO: Write a proper emconfigure build file.
##       This is a quick and dirty build script.

import os, shutil
from os.path import join

SRC_DIR     = '../../src/'
JS_API_PATH = './io_api.js'
TARGET_DIR  = '../static/'
TARGET_NAME = 'pocketlang.html'
PAGE_SCRIPT = 'try_now.js'

def main():
  sources = ' '.join(collect_source_files())
  include = '-I' + fix_path(join(SRC_DIR, 'include/'))
  output  = join(TARGET_DIR, TARGET_NAME)
  exports = "\"EXPORTED_RUNTIME_METHODS=['ccall','cwrap']\""
  js_api  = JS_API_PATH
  
  cmd = f"emcc {include} main.c {sources} -o {output} " +\
        f"-s {exports} --js-library {js_api}"
  
  print(cmd)
  os.system(cmd)
  
  shutil.copyfile(PAGE_SCRIPT, join(TARGET_DIR,PAGE_SCRIPT))
  os.remove(output) ## Not using the generated html file.
  
  
def fix_path(path):
  return path.replace('\\', '/')

def collect_source_files():
  sources = []
  for file in os.listdir(SRC_DIR):
    if not os.path.isfile(join(SRC_DIR, file)): continue
    if file.endswith('.c'):
      source = fix_path(join(SRC_DIR, file))
      sources.append(source)
  return sources

if __name__ == '__main__':
  main()
