from markdown import markdown
from os.path import join
import os, sys, shutil

## Usage:
##   to generate pages : python generate.py
##   to clean pages    : python generate.py (-c, --clean)

TEMPLATE_PATH = 'static/template.html'
ROOT_URL      = 'file:///C:/dev/pocketlang/docs/build/'
#ROOT_URL     = 'https://thakeenathees.github.io/pocketlang/'

## Home page should be in the SOURCE_DIR.
HOME_PAGE  = 'home.md'
SOURCE_DIR = 'pages/'
TARGET_DIR = 'build/'
STATIC_DIR = 'static/'

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
	
	#shutil.rmtree(TARGET_DIR, ignore_errors=True)
	shutil.copytree(STATIC_DIR, join(TARGET_DIR, STATIC_DIR))
	open(join(TARGET_DIR, '.nojekyll'), 'w').close()
	
	## Initialize the template and navigation.
	template = ''
	navigation = generate_navigation()
	with open(TEMPLATE_PATH, 'r') as f:
		template = f.read()
		
	## Generate the home page.
	generate_page(join(SOURCE_DIR, HOME_PAGE),
		          join(TARGET_DIR, 'index.html'),
		          template, navigation)
	
	## Generate the content pages.
	for _dir in os.listdir(SOURCE_DIR):
		if os.path.isfile(join(SOURCE_DIR, _dir)):
			if _dir == HOME_PAGE: continue
			
			path = join(SOURCE_DIR, _dir)
			dst  = join(TARGET_DIR, _dir)
			generate_page(path, dst, template, navigation)
		else:
			for file in os.listdir(join(SOURCE_DIR,_dir)):
				assert(file.endswith('.md'))
				path = join(SOURCE_DIR, _dir, file)
				dst  = join(TARGET_DIR, _dir, file.replace('.md', '.html'))
				generate_page(path, dst, template, navigation)
	pass

def generate_navigation():
	navigation = ''
	for _dir in os.listdir(SOURCE_DIR):
		if os.path.isfile(join(SOURCE_DIR, _dir)):
			if _dir == HOME_PAGE: continue
			assert() ## TODO:
		else:
			navigation += '<div class="navigation">\n'
			navigation += '<h3><strong>%s</strong></h3>\n' % (_dir)
			navigation += '<ul class="menu">\n'
			
			for file in os.listdir(join(SOURCE_DIR, _dir)):
				assert(file.endswith('.md'))
				
				link = join(ROOT_URL, _dir, file.replace('.md', '.html'))
				title = file.replace('.md', '')
				navigation += '<li><a href="%s">%s</a></li>\n' % (link, title)
			navigation += '</ul>\n'
			navigation += '</div>\n'
			
	return navigation

def generate_page(src, dst, template, navigation):
	assert(src.endswith('.md'))
	
	_dir = os.path.dirname(dst)
	if _dir not in ('.', './', '') and not os.path.exists(_dir):
		os.makedirs(os.path.dirname(dst))

	## get the title.
	title = os.path.basename(src).replace('.md', '').title()
	title += ' - PocketLang'

	## get the relative static dir.
	static_dir = os.path.relpath(join(TARGET_DIR,STATIC_DIR), _dir)
	static_dir = static_dir.replace('\\', '/')
	if static_dir[-1] != '/':
		static_dir += '/'

	content = ''
	with open(src, 'r') as home_md:
		content = md2content(home_md.read())
		
	ctx = {
		'{{ TITLE }}'     : title,
		'{{ NAVIGAION }}' : navigation,
		'{{ CONTENT }}'   : content,
		
		'{{ ROOT_URL }}'  : ROOT_URL + 'index.html',
		'{{ STATIC_DIR }}': static_dir,
	}
	
	page = template
	for key, value in ctx.items():
		page = page.replace(key, value)
		
	with open(dst, 'w') as f:
		f.write(page)

def md2content(md_text):
	content = markdown(md_text, extensions=['codehilite', 'fenced_code'])
	
	## FIXME: I should create a pygment lexer.
	## A dirty way to inject our keyword (to ruby's).
	addnl_keywords = [
		'null', 'from', 'import', 'as', 'func', 'native', 'elif', 'continue'
	]
	not_keyword = [
		 'alias', 'begin', 'case', 'class', 'next', 'nil', 'redo', 'rescue',
		 'retry', 'elsif', 'ensure', 'undef', 'unless', 'super', 'until', 'when',
		  'defined',
  ]

	for kw in addnl_keywords:
		content = content.replace(	'<span class="n">%s</span>' % kw, 
									'<span class="k">%s</span>' % kw)
	for nk in not_keyword:
		content = content.replace(	'<span class="k">%s</span>' % nk, 
									'<span class="n">%s</span>' % nk)
	return content

if __name__ == '__main__':
	main()
	print('Static pages generated.')

