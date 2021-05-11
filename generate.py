from markdown import markdown
import os, sys, shutil

## Usage:
##   to generate pages : python generate.py
##   to clean pages    : python generate.py (-c, --clean)

TEMPLATE_PATH = 'static/template.html'
ROOT_URL      = 'file:///C:/dev/pocketlang/docs/'
ROOT_URL      = 'https://thakeenathees.github.io/pocketlang/'

## Home page should be in the SOURCE_DIR.
HOME_PAGE  = 'home.md'
SOURCE_DIR = 'pages/'
TARGET_DIR = 'build/'

def main():
	
	## Initialize the template and navigation.
	template = ''
	navigation = generate_navigation()
	with open(TEMPLATE_PATH, 'r') as f:
		template = f.read()
		
	## Generate the home page.
	generate_page(os.path.join(SOURCE_DIR, HOME_PAGE),
		'index.html', template, navigation)
	
	## Generate the content pages.
	for _dir in os.listdir(SOURCE_DIR):
		if os.path.isfile(os.path.join(SOURCE_DIR, _dir)):
			if _dir == HOME_PAGE: continue
			
			path = os.path.join(SOURCE_DIR, _dir)
			dst  = os.path.join(TARGET_DIR, _dir)
			generate_page(path, dst, template, navigation)
		else:
			for file in os.listdir(os.path.join(SOURCE_DIR,_dir)):
				assert(file.endswith('.md'))
				path = os.path.join(SOURCE_DIR, _dir, file)
				dst  = os.path.join(TARGET_DIR, _dir, file.replace('.md', '.html'))
				generate_page(path, dst, template, navigation)
	pass

def generate_navigation():
	navigation = ''
	for _dir in os.listdir(SOURCE_DIR):
		if os.path.isfile(os.path.join(SOURCE_DIR, _dir)):
			if _dir == HOME_PAGE: continue
			assert() ## TODO:
		else:
			navigation += '<div class="navigation">\n'
			navigation += '<h3><strong>%s</strong></h3>\n' % (_dir)
			navigation += '<ul class="menu">\n'
			
			for file in os.listdir(os.path.join(SOURCE_DIR, _dir)):
				assert(file.endswith('.md'))
				
				link = os.path.join(ROOT_URL, TARGET_DIR, _dir, file.replace('.md', '.html'))
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
	static_dir = os.path.relpath('static/', _dir)
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
	
	if len(sys.argv) >= 2 and sys.argv[1] in ('-c', '--clean'):
		shutil.rmtree(TARGET_DIR, ignore_errors=True)
		if os.path.exists('index.html'):
			os.remove('index.html')
		print('Generated files were cleaned.')
	else:
		main()
		print('Static pages generated.')

