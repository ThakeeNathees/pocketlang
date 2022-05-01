##
## Copyright (c) 2020-2022 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License
##

## A tiny script to download and place premake binary in this path.
## Primarly used to generate Visual Studio project files. Feel free
## add other build script and platforms.

import urllib.request
import os, platform
import tempfile, zipfile, tarfile
from os.path import abspath, dirname, join

THIS_PATH = abspath(dirname(__file__))

PREMAKE_PLATFORM_URL = {
  "Windows": "https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-windows.zip",
  #"Linux": TODO,
  #"Darwin": TODO,
}

def main():
  system = platform.system()
  if system not in PREMAKE_PLATFORM_URL:
    error_exit(f"Platform {system} is currently not supported.\n" +\
    "(You can modify this script to add your platform and contribute).")
  
  premake_url = PREMAKE_PLATFORM_URL[system]
  tmpdir = tempfile.mkdtemp()
  zip_path = join(tmpdir, 'premake5.zip')
  
  with urllib.request.urlopen(premake_url) as binary_zip:
    with open(zip_path, 'wb') as f:
      f.write(binary_zip.read())
  premake_zip = zipfile.ZipFile(zip_path, 'r')
  premake_zip.extractall(THIS_PATH)
  
  print("premake5 downloaded successfully.")

## prints an error message to stderr and exit immediately.
def error_exit(msg):
  print("Error:", msg, file=sys.stderr)
  sys.exit(1)

if __name__ == "__main__":
  main()
