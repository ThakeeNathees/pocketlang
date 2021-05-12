#!python
## Copyright (c) 2021 Thakee Nathees
## Licensed under: MIT License

## TODO: Write a proper emconfigure build file.
##       This is a quick and dirty build script.

import os, shutil
from os.path import join

SRC_DIR     = '../../src/'
JS_API_PATH = './js_api.js'
TARGET_DIR  = '../static/'
TARGET_NAME = 'try_now.html'

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
  
  ## TODO: copy output to static dir.
  os.remove(output) ## Not using the generated html file.
  
  
def fix_path(path):
  return path.replace('\\', '/')

def collect_source_files():
  sources = []
  
  def add_all(root, sources):
    for file in os.listdir(root):
      if not os.path.isfile(join(root, file)): continue
      if file.endswith('.c'):
        source = fix_path(join(root, file))
        sources.append(source)

  add_all(SRC_DIR, sources)
  add_all(join(SRC_DIR, 'buffers/'), sources)
  return sources

if __name__ == '__main__':
  main()