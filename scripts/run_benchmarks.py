#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License

import re, os, sys
import subprocess, platform
from shutil import which
from os.path import join, abspath, dirname, relpath, exists

## Absolute path of this file's directory.
THIS_PATH = abspath(dirname(__file__))

## Output debug cli executable's directory.
POCKET_BINARY_DIR = abspath(join(THIS_PATH, "../build/Release/bin/"))

## Pocket lang root directory. All the listed paths bellow are relative to
## the BENCHMARKS_DIR.
BENCHMARKS_DIR = abspath(join(THIS_PATH, "../tests/benchmarks"))

## A list of benchmark directories, relative to BENCHMARKS_DIR
BENCHMARKS = (
  "factors",
  "fib",
  "list",
  "loop",
  "primes",
)

## Map the files extension with it's interpreter. (executable, extension).
INTERPRETERS = {
  'pocketlang' : ('pocket',  '.pk'),
  'python'     : ('python3', '.py'),
  'wren'       : ('wren',    '.wren'),
  'ruby'       : ('ruby',    '.rb'),
  'lua'        : ('lua',     '.lua'),
  ## Javascript on Node is using V8 that compiles the function before calling it
  ## which makes it way faster than every other language in this list, we're
  ## only comparing byte-code interpreted VM languages. Node is the odd one out.
  #'javascript' : ('node',    '.js'),
}

## The html template to display the report.
HTML_TEMPLATE = "template.html"

def main():
  results = dict()

  update_interpreters()

  for benchmark in BENCHMARKS:
    print_title(benchmark.title())
    for lang in INTERPRETERS:
      interp, ext = INTERPRETERS[lang]
      source = abspath(join(BENCHMARKS_DIR, benchmark, benchmark + ext))
      if not exists(source): continue
      print(" %-10s : "%lang, end=''); sys.stdout.flush()
      result = subprocess.run([interp, source],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
      time = get_time(result.stdout.decode('utf8'))
      print('%.6fs' % float(time))

      if benchmark not in results:
        results[benchmark] = []
      results[benchmark].append([lang, time])

  display_results(results)

## This will return the path of the pocket binary (on different platforms).
## The debug version of it for enabling the assertions.
def get_pocket_binary(name, fail):
  system = platform.system()
  if system not in ("Windows", "Linux", "Darwin"):
    error_exit("Unsupported platform")
  binary = join(POCKET_BINARY_DIR, name)
  if system == "Windows": binary += ".exe"
  if not exists(binary):
    if fail: error_exit(f"Pocket interpreter not found at: '{binary}'")
    else: return None
  return binary

## Set the pocketlang path for the current system to the compiled output.
def update_interpreters():
  pocket = get_pocket_binary("pocket", True)
  pocket_older = get_pocket_binary("pocket_older", False)

  global INTERPRETERS
  INTERPRETERS['pocketlang'] = (pocket, '.pk')

  ## Add if older version of pocketlang if exists.
  if pocket_older is not None:
    INTERPRETERS['pk-older'] = (pocket_older, '.pk')

  if 'python' in INTERPRETERS and platform.system() == "Windows":
    INTERPRETERS['python'] = ('python', '.py')
  
  missing = []
  for lang in INTERPRETERS:
    interpreter = INTERPRETERS[lang][0]
    print('%-27s' % ('  Searching for %s ' % lang), end='')
    if which(interpreter):
      print(f"-- {colmsg('found', COL_GREEN)}")
    else:
      print(f"-- {colmsg('missing', COL_YELLOW)}")
      missing.append(lang)
  for miss in missing:
    INTERPRETERS.pop(miss)

## Match 'elapsed: <time>s' from the output of the process and return it.
def get_time(result_string):
  time = re.findall(r'elapsed:\s*([0-9\.]+)\s*s', result_string, re.MULTILINE)
  if len(time) != 1:
    print(f'\n\'elapsed: <time>s\' {colmsg("No match found", COL_RED)}.')
    sys.exit(1)
  return float(time[0])

## Generate a report at 'report.html'.
def display_results(results):

  for benchmark in results:
    results[benchmark].sort(key=lambda x : x[1])
    max_time = 0
    for lang, time in results[benchmark]:
      max_time = max(max_time, time)

    ## Add the width for the performance bar.
    for entry in results[benchmark]:
      time = entry[1]
      entry.append(time/max_time * 100)

  disp = ""
  for benchmark in results:
    disp += f'<h1 class="benchmark-title">{benchmark.title()}</h1>\n'
    disp += '<table>\n'
    disp += '<tbody>\n'
    for lang, time, width in results[benchmark]:
      class_ = "performance-bar"
      if lang == 'pocketlang':
        class_ += " pocket-bar"
        lang = 'pocket' ## Shorten the name.

      disp += '<tr>\n'
      disp += f'  <th>{lang}</th>\n'
      disp += f'  <td><div class="{class_}" style="width:{width}%">'
      disp += f'{"%.2f"%time}s</div></td>\n'
      disp += '</tr>\n'
    disp += '</tbody>\n'
    disp += '</table>\n'
    disp += '\n'

  global HTML
  html = HTML.replace('{{ REPORT }}', disp)
  report_path = abspath(join(THIS_PATH, 'report.html'))
  with open(report_path, 'w') as out:
    out.write(html)
  print()
  print(colmsg('Report Generated:', COL_GREEN), report_path)

## ----------------------------------------------------------------------------
## HTML REPORT
## ----------------------------------------------------------------------------

HTML = '''
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Document</title>

  <style>
* {
  padding: 0;
  margin: 0;
  box-sizing: border-box;
  font-family: sans-serif;
}

#main {
  max-width: 600px;
  margin: 50px auto;
}

.benchmark-title {
  text-align: center;
  margin: 40px auto 20px auto;
}

table {
  width: 100%;
}

th {
  font-weight: normal;
  text-align: right;
  width: 100px;
}

.performance-bar {
  background-color: #1471c8;
  margin: 1px 10px;
  color:white;
  padding: 2px;
}

.pocket-bar {
  background-color: #02509B;
}
  </style>
</head>

<body>

  <div id="main">

    {{ REPORT }}

  </div>
  
</body>
</html>
'''

## ----------------------------------------------------------------------------
## PRINT FUNCTIONS
## ----------------------------------------------------------------------------

def _print_sep():
  print("-------------------------------------")

def print_title(title):
  _print_sep()
  print(" %s " % title)
  _print_sep()

## prints an error message to stderr and exit immediately.
def error_exit(msg):
  print("Error:", msg, file=sys.stderr)
  sys.exit(1)
  
## Simple color logger --------------------------------------------------------
## https://stackoverflow.com/a/70599663/10846399

def RGB(red=None, green=None, blue=None,bg=False):
    if(bg==False and red!=None and green!=None and blue!=None):
        return f'\u001b[38;2;{red};{green};{blue}m'
    elif(bg==True and red!=None and green!=None and blue!=None):
        return f'\u001b[48;2;{red};{green};{blue}m'
    elif(red==None and green==None and blue==None):
        return '\u001b[0m'
COL_NONE = RGB()
COL_RED = RGB(220, 100, 100)
COL_GREEN = RGB(15, 160, 100)
COL_BLUE = RGB(60, 140, 230)
COL_YELLOW = RGB(220, 180, 20)
COL_WHITE = RGB(255, 255, 255)

def colmsg(msg, color):
  return f"{color}{msg}{COL_NONE}"

if __name__ == "__main__":
  os.system('')
  main()
