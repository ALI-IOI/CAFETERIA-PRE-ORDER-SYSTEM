# Builds the online demo for GitHub Pages from web/ (the real pages, unchanged in
# the repo): absolute paths get the Pages base path, and pages-demo/demo-api.js
# (an in-browser stand-in for the C++ server) loads before js/api.js.
# Usage: python pages-demo/build.py <output dir> [base path, default /cafeteria-pre-order-system/]
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.abspath(sys.argv[1])
base = sys.argv[2] if len(sys.argv) > 2 else '/cafeteria-pre-order-system/'
if not (base.startswith('/') and base.endswith('/')):
    sys.exit('base path must start and end with /')

if os.path.isdir(out):
    shutil.rmtree(out)
shutil.copytree(os.path.join(ROOT, 'web'), out)
shutil.copy(os.path.join(ROOT, 'pages-demo', 'demo-api.js'), os.path.join(out, 'demo-api.js'))
open(os.path.join(out, '.nojekyll'), 'w').close()

# "/css/app.css", '/menu.html#tray', url("/img/x.svg") ... -> base-prefixed. A bare
# "/" is text in some scripts (dates, "3/100"), so only its two link forms change.
ABS = re.compile(r'''(["'`(])/((?:css|js|img|fonts|staff|images|api)/[^"'`)\s]*|[a-z-]+\.html(?:#[^"'`)\s]*)?)(?=["'`)])''')
HOME_LINKS = [('href="/"', 'href="{base}"'), ('wireLogout("logout", "/")', 'wireLogout("logout", "{base}")')]
NOTE_CSS = '''
/* ---- online demo notice (GitHub Pages build only) ---- */
.demo-note { background: #FFF8D6; color: var(--ink); border-bottom: 1px solid var(--line); font-size: 0.85rem; padding: 0.5rem var(--space); }
.demo-note p { max-width: 1180px; margin: 0 auto; }
.demo-note .linkbtn { color: var(--accent-text); font-weight: 600; min-height: 44px; }
'''

changed = 0
for folder, _, files in os.walk(out):
    for name in files:
        path = os.path.join(folder, name)
        if not name.endswith(('.html', '.js', '.css')) or name == 'demo-api.js':
            continue
        text = open(path, encoding='utf8').read()
        new = ABS.sub(lambda m: m.group(1) + base + m.group(2), text)
        for old, repl in HOME_LINKS:
            new = new.replace(old, repl.format(base=base))
        if name.endswith('.html'):
            new = new.replace('<script src="' + base + 'js/api.js"></script>',
                              '<script src="' + base + 'demo-api.js"></script>\n  <script src="' + base + 'js/api.js"></script>', 1)
            new = new.replace('<title>', '<title>Demo · ', 1)
        if name == 'app.css':
            new += NOTE_CSS
        if new != text:
            open(path, 'w', encoding='utf8', newline='\n').write(new)
            changed += 1

pages = [f for f in os.listdir(out) if f.endswith('.html')] + ['staff/' + f for f in os.listdir(os.path.join(out, 'staff')) if f.endswith('.html')]
missing = [p for p in pages if 'demo-api.js' not in open(os.path.join(out, p), encoding='utf8').read()]
if missing:
    sys.exit('demo-api.js not loaded on: ' + ', '.join(missing))
print('built %s (base %s): %d files rewritten, %d pages' % (out, base, changed, len(pages)))
