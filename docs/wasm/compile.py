import os, shutil, platform
from os.path import join, isdir, abspath, dirname, exists
from shutil import which

## The absolute path of this file's directory, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(dirname(__file__))

POCKET_ROOT = join(THIS_PATH, "../../../pocketlang/src/")
JS_API_PATH = join(THIS_PATH, "io_api.js")
MAIN_C = join(THIS_PATH, "main.c")
TARGET_DIR = join(THIS_PATH, "../static/wasm/")
TARGET_NAME = "pocketlang.html"

def main():

  ## Check the compiler is available.
  if not which("emcc"):
    msg = "emcc Not found."
    if platform.system().lower() == "windows":
      msg += " (Did you forget to run 'call emsdk_env.bat')"
    print(msg)
    exit(1)

  ## Make the target dir (which is gitignored and couldn't be exists).
  if not exists(TARGET_DIR):
    os.mkdir(TARGET_DIR)

  sources = ' '.join(collect_source_files())
  include = '-I' + join(POCKET_ROOT, 'include/')
  output  = join(TARGET_DIR, TARGET_NAME)
  exports = "\"EXPORTED_RUNTIME_METHODS=['ccall','cwrap']\""
  js_api  = JS_API_PATH

  cmd = f"emcc {include} {MAIN_C} {sources} -o {output} " +\
        f"-s {exports} --js-library {js_api}"
  print(cmd)
  os.system(cmd)

def collect_source_files():
  sources = []
  for dir in ('core/', 'libs/'):
    for file in os.listdir(join(POCKET_ROOT, dir)):
      if isdir(file): continue
      if file.endswith('.c'): sources.append(join(POCKET_ROOT, dir, file))
  return sources

if __name__ == "__main__":
  main()
