#!python
## Copyright (c) 2021 Thakee Nathees
## Licensed under: MIT License

from markdown import markdown
from os.path import join
import os, sys, shutil, re

## TODO: This is a quick and dirty script to generate html
##       from markdown. Refactor this file in the future.

## Usage:
##   to generate pages : python generate.py
##   to clean pages    : python generate.py (-c, --clean)

TEMPLATE_PATH = 'static/template.html'
ROOT_URL      = 'https://thakeenathees.github.io/pocketlang/'

## Home page should be in the SOURCE_DIR.
HOME_PAGE  = 'home.md'
TRY_PAGE   = 'try-it-now.html'
SOURCE_DIR = 'pages/'
TARGET_DIR = 'build/'
STATIC_DIR = 'static/'

## Additional source files of wasm try online page.
WASM_SOURCE_FILES = '''\
  <script type="text/javascript" src="{{ STATIC_DIR }}codejar/codejar.js"></script>
    <script type="text/javascript" src="{{ STATIC_DIR }}codejar/linenumbers.js"></script>
    <link rel="stylesheet" type="text/css" href="{{ STATIC_DIR }}codejar/style.css" />
    
    <script type="text/javascript" src="{{ STATIC_DIR }}prism/prism.js"></script>
    <link rel="stylesheet" type="text/css" href="{{ STATIC_DIR }}prism/prism.css" />
	
  <script type="text/javascript" src="{{ STATIC_DIR }}try_now.js"></script>
'''

## Navigation pages in order. Should match the path names.
## Any file/folder name shouldn't contain white space.
PAGES = [
	('Getting-Started', [
		TRY_PAGE,
		'learn-in-15-minutes.md',
		'build-from-source.md',
		'contributing.md',
	]),
	
	('Language-API', [
		'variables.md',
		'functions.md',
		'fibers.md',
		'modules.md',
	]),
]

def new_context():
	return {
		'{{ TITLE }}'      : '',
		'{{ NAVIGATION }}' : '',
		'{{ CONTENT }}'    : '',
		'{{ HOME_URL }}'   : '',
		'{{ STATIC_DIR }}' : '',
	}

def main():
	
	## Remove generated files and create empty target dir with static files.
	if os.path.exists(TARGET_DIR):
		remove_ignore = ( '.git', )
		for _dir in os.listdir(TARGET_DIR):
			if _dir in remove_ignore: continue
			if os.path.isdir(join(TARGET_DIR,_dir)):
				shutil.rmtree(join(TARGET_DIR, _dir))
			else:
				os.remove(join(TARGET_DIR, _dir))

	shutil.copytree(STATIC_DIR, join(TARGET_DIR, STATIC_DIR))
	open(join(TARGET_DIR, '.nojekyll'), 'w').close()
	
	## Initialize the template and navigation.
	template = ''
	navigation = generate_navigation()
	with open(TEMPLATE_PATH, 'r') as f:
		template = f.read()
		
	## Generate the home page.
	index_html = join(TARGET_DIR, 'index.html')
	ctx = generate_page_context(join(SOURCE_DIR, HOME_PAGE), index_html, navigation)
	write_page(ctx, template, index_html)
	
	for entry in PAGES: ## entry = ('dirname', [files...])
		_dir = entry[0]
		for file in entry[1]:
			ext = get_validated_ext(file)
			path = join(SOURCE_DIR, _dir, file)
			
			dst = ''; path_prefix = _dir.lower().replace(' ', '-') + '-'
			if ext == '.md':
				dst = join(TARGET_DIR, path_prefix + file.replace('.md', '.html'))
			else:
				dst = join(TARGET_DIR, path_prefix + file)
			ctx = generate_page_context(path, dst, navigation)
			
			_template = template
			if file == TRY_PAGE:
				_template = template.replace('{{ WASM_SOURCE_FILES }}', WASM_SOURCE_FILES)
			
			write_page(ctx, _template, dst)
	pass

def generate_navigation():
	navigation = ''
	for entry in PAGES:
		_dir = entry[0]
		navigation += '<div class="navigation">\n'
		navigation += '<h3><strong>%s</strong></h3>\n' % (_dir.replace('-', ' ').title())
		navigation += '<ul class="menu">\n'
		
		for file in entry[1]:
			ext = get_validated_ext(file)
			
			link = '' ## Assuming that file name don't contain '.md' at the middle.
			
			path_prefix = _dir.lower().replace(' ', '-') + '-'
			if ext == '.md':
				link = join(ROOT_URL, path_prefix + file.replace('.md', '.html'))
			else:
				link = join(ROOT_URL, path_prefix + file)
			link = link.replace('\\', '/')
			
			title = file.replace(ext, '').replace('-', ' ').title()
			navigation += '<li><a href="%s">%s</a></li>\n' % (link, title)
			
		navigation += '</ul>\n'
		navigation += '</div>\n'
		
	return navigation
	

def generate_page_context(src, dst, navigation):
	title = path_to_title(src)
	static_dir = relative_static_dir(dst)
	content = path_to_content(src)
	ctx = new_context()
	ctx[ '{{ TITLE }}'      ] = title
	ctx[ '{{ NAVIGATION }}' ] = navigation
	ctx[ '{{ CONTENT }}'    ] = content
	ctx[ '{{ HOME_URL }}'   ] = ROOT_URL + 'index.html'
	ctx[ '{{ STATIC_DIR }}' ] = static_dir
	return ctx;

def get_validated_ext(path)	:
	ext = ''
	if path.endswith('.md'): ext = '.md'
	elif path.endswith('.html'): ext = '.html'
	else: raise Exception('Expected .md / .html file.')
	return ext

## Get the title from the src path.
def path_to_title(path):
	ext = get_validated_ext(path)
	title = os.path.basename(path).replace(ext, '').title()
	title += ' - PocketLang'
	return title

## Return the static dir relative path.
def relative_static_dir(dst):

	return STATIC_DIR ## No more relative paths.
	
	_dir = os.path.dirname(dst)
	static_dir = os.path.relpath(join(TARGET_DIR, STATIC_DIR), _dir)
	static_dir = static_dir.replace('\\', '/')
	if static_dir[-1] != '/':
		static_dir += '/'
	return static_dir

## Generate html content from the markdown source path.
## If the path is an .html file return it's content.
def path_to_content(src):
	
	text = ''
	with open(src, 'r') as f:
		text = f.read()
		
	## If html file we're done.
	if get_validated_ext(src) == '.html':
		return text
	
	assert(src.endswith('.md'))
	text = custom_md_override(text)
	content = markdown(text, extensions=['codehilite', 'fenced_code'])
	
	## A wakey way to inject html overrides to highlight out language
	## I'm not focusing on generating the pages and this is a wakey way to
	## do so. This should be done with a good static page generater instead
	## of this script.
	return custom_html_override(src, content)

## Inject our custom markdown text override.
def custom_md_override(text):

	## Add html anchor.
	for pre in ('#', '##', '###'):
		pattern = '(^' + pre + r' \s*%%(.*)%%\n)'
		for match, title in re.findall(pattern, text, flags=re.MULTILINE):
			link = title.strip().lower().replace(' ', '-')
			text = text.replace(match, f'{pre} {title} <a href="#{link}" name="{link}" class="anchor">#</a>')
	return text

## Inject our custom html overrides.
def custom_html_override(src, content):
	## FIXME: I should create a pygment lexer.
	## A dirty way to inject our keyword (to ruby's).
	addnl_keywords = [
		'null', 'from', 'import', 'as', 'func', 'native', 'continue'
	]
	not_keyword = [
		 'alias', 'begin', 'case', 'class', 'next', 'nil', 'redo', 'rescue',
		 'retry', 'ensure', 'undef', 'unless', 'super', 'until', 'when',
		  'defined',
	]

	for kw in addnl_keywords:
		content = content.replace('<span class="n">%s</span>' % kw, 
								   '<span class="k">%s</span>' % kw)
	for nk in not_keyword:
		content = content.replace('<span class="k">%s</span>' % nk, 
								  '<span class="n">%s</span>' % nk)
	
	## codehilite mark the compilation command as error.
	content = content.replace('<span class="err">', '<span>')
	
	return content

def write_page(ctx, template, dst):
	_dir = os.path.dirname(dst)
	if _dir not in ('.', './', '') and not os.path.exists(_dir):
		os.makedirs(os.path.dirname(dst))
		
	page = template
	for key, value in ctx.items():
		page = page.replace(key, value)
		
	page = page.replace('{{ WASM_SOURCE_FILES }}', '')
		
	with open(dst, 'w') as f:
		f.write(page)
	

if __name__ == '__main__':
	_local = False
	if len(sys.argv) >= 2:
		if sys.argv[1] == 'local':
			_local = True
			#ROOT_URL = 'http://localhost:8000/'
	
	ROOT_URL = '' ## No more nested directory pages.
	
	main()
	
	## Write a batch file to start the server in windows.
	if _local and os.name == 'nt':
		with open(join(TARGET_DIR, 'server.bat'), 'w') as f:
			f.write('python -m http.server 8000')
			
	print('Static pages generated' +\
	      ('for localhost:8000.' if _local else '.'))

