## Make sure you have installed markdown library (pip install Markdown)
## and the python version is 3.6+.

import os, sys, re, json, html
from os.path import abspath, dirname, exists, isdir, join
from shutil import copyfile, copytree, rmtree
from markdown import markdown

POCKETLANG_VERSION = "v0.1.0"

## Page contnet will be ordered as the below list. Each entry in the DOC_PAGES
## is called a section (which will be it's own folder) and each entry in the
## section is a page (which will be in it's own file). Additionally each page
## has it's own topic that'll be generated as h2 tags.
##
## A generated context of the function collect_doc_pages() will be in the form
## of :- { section : [ page : (html, [topic]) ] }
## where html is the html version of the markdown file.
##
DOC_PAGES = {
  "Reference" : [
    "Getting-Started",
    "Cheat-Sheet",
    "Build-From-Source",
    "Installation",
  ],
  "Api-Docs" : [
    "Fiber",
    "Module",
  ],
}

## The absolute path of this file's directory, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(dirname(__file__))

BUILD_DIR = join(THIS_PATH, "build")
STATIC_DIR = join(THIS_PATH, "static")
MARKDOWN_DIR = join(THIS_PATH, "markdown")
DOCS_URL_PATH = f"docs/{POCKETLANG_VERSION}/" ## Html generated at docs/* path.

def main():
  check_wasm()
  clean()
  make_build_dir()
  gen_home_page()
  context = collect_doc_pages()
  generate_pages(context)
  print("Docs generated successfully")

## INTERNAL ###################################################################

## Opens the file at the path read it and return it's content.
def read(path):
  with open(path, 'r') as fp:
    return fp.read()

def check_wasm():
  if not exists(join(STATIC_DIR, 'wasm/')):
    print("[Warning] pocketlang web assembly files weren't generated.")
    print("          to compile, see docs/wasm/README.md")

## Remove all generated files at the build path.
def clean():
  REMOVE_IGNORE =('.git',)
  if not exists(BUILD_DIR): return
  for item in os.listdir(BUILD_DIR):
    if item in REMOVE_IGNORE: continue
    item = join(BUILD_DIR, item)
    if isdir(item): rmtree(item)
    else: os.remove(item)

## Generate necessary folder and files to the build dir, copy static folder and
## prepare for the generation.
def make_build_dir():
  ## Create '.nojekyll' for github pages.
  if not exists(BUILD_DIR): os.makedirs(BUILD_DIR)
  open(join(BUILD_DIR, '.nojekyll'), 'w').close()
  copytree(STATIC_DIR, join(BUILD_DIR, "static"))

def gen_home_page():
  template = read(join(THIS_PATH, "index.html"))
  template = template.replace("{{ STATIC_DIR }}", "./static/")
  template = template.replace("{{ DOCS_URL }}", DOCS_URL_PATH)
  with open(join(BUILD_DIR, 'index.html'), 'w') as fp:
    fp.write(template)

## FIXME: wirte a better md -> html compiler (or use some libraries).
## Compile the markdown files to html. and return the a tuple of (html, topics)
## where topics are h2 tag headers inside the markdown files.(h1 tag is
## reserved for the page heading).
def compile_markdown(md):
  topics = []
  md_new = ""
  in_code = False

  for line in md.splitlines():
    stripped_line = line.strip()

    ## Get topics.
    if stripped_line.startswith("## "):
      topic = stripped_line[3:]
      topics.append(topic)
      md_new += f'<h2 class="topic-title">{topic} <a href="#{topic}" ' +\
                f'id="{topic}" class="anchor">#</a></h2>\n'

    ## Parse codeblocks (markdown library doesn't support this extended
    ## markdown syntax).
    elif not in_code and stripped_line.startswith("```"):
      ## Expected a lang name and EOL.
      lang = stripped_line[3:]
      md_new += f'<pre><code class="highlight {lang}">\n'
      in_code = True

    ## End of codeblock.
    elif in_code and stripped_line.startswith('```'):
      md_new += '</code></pre>\n'
      in_code = False

    else:
      if in_code: line = html.escape(line)
      md_new += line + "\n"

  return markdown(md_new), topics

## Collect all markdown and generate the context (context is mentioned at the
## top of the script).
def collect_doc_pages():
  context = dict()
  for section in DOC_PAGES:
    context[section] = dict()
    for file_name in DOC_PAGES[section]:
      md = read(join(MARKDOWN_DIR, section, file_name + '.md'))
      context[section][file_name] = compile_markdown(md)
  return context

## Generate navigation tree html tags with the tree information defined at the
## markdown/tree.json file (since python 3.6 dicionaries are ordered we're not
## using the OrderedDict here).
def generate_navtree(context):
  gen = ""; indentation = '  ' * 4
  def write(html_line):
    nonlocal gen; nonlocal indentation
    gen += indentation + html_line + '\n'

  for section in context:
    write('<li>')
    write('  <div class="nav-section">')
    write('    <p> %s </p>' % section.replace('-', ' '))
    write('    <svg class="collapse-arrow" role="img" class="opacity-60" viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path fill-rule="evenodd" d="M12.78 6.22a.75.75 0 010 1.06l-4.25 4.25a.75.75 0 01-1.06 0L3.22 7.28a.75.75 0 011.06-1.06L8 9.94l3.72-3.72a.75.75 0 011.06 0z"></path></svg>')
    write('  </div>')
    write('  <ul class="nav-topics">')
    for page in context[section]:
      write('    <li><a href="%s"> %s </a></li>' % (
        f"../{section}/{page}.html",
        page.replace('-', ' ')))
    write('  </ul>')
    write('</li>')
  return gen

## Generate table of content entries for the page.
def generate_toc_entries(topics):
  gen = ""
  for topic in topics:
    gen += f'<li><a href="#{topic}">{topic.replace("-", " ")}</a></li>\n'
  return gen

## Build the template page with the generated context.
def generate_pages(context):
  navtree = generate_navtree(context)

  template = read(join(MARKDOWN_DIR, 'template.html'))
  template = template.replace("{{ NAVIGATION_TREE }}", navtree)
  template = template.replace("{{ LOGO_WHITE }}",
                              read(join(STATIC_DIR, "img/logo-white.svg")))
  template = template.replace("{{ URL_POCKET_HOME }}", "../../index.html")

  for section in context:
    section_dir = join(BUILD_DIR, DOCS_URL_PATH, section)
    os.makedirs(section_dir)
    for page in context[section]:
      content_html, topics = context[section][page]
      toc_entries = generate_toc_entries(topics)
      html = template ## "copy" the template string for the page.
      html = html.replace("{{ PAGE_CONTENT }}", content_html)
      html = html.replace("{{ PAGE_TITLE }}", page.replace('-', ' '))
      html = html.replace("{{ TOC_ENTRIES }}", toc_entries)
      html = html.replace("{{ STATIC_DIR }}", '../../../static/')
      with open(join(section_dir, page + '.html'), 'w') as fp:
        fp.write(html)

if __name__ == "__main__":
  main()

