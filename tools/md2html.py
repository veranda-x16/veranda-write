import sys, re, html
def inline(t):
    t = html.escape(t)
    t = re.sub(r'!\[[^\]]*\]\([^)]*\)', '', t)
    t = re.sub(r'\[([^\]]+)\]\([^)]*\)', r'\1', t)
    t = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', t)
    t = re.sub(r'(?<![*\w])\*([^*]+)\*(?!\w)', r'<i>\1</i>', t)
    t = re.sub(r'`([^`]+)`', r'<code>\1</code>', t)
    return t
title, files = sys.argv[1], sys.argv[2:]
out = ['<html><head><meta charset="utf-8"><title>%s</title></head><body>' % html.escape(title), '<h1>%s</h1>' % html.escape(title)]
for f in files:
    lines = open(f, encoding='utf-8').read().splitlines()
    i = 0; para = []; lst = None
    def flush():
        global para
        if para: out.append('<p>%s</p>' % inline(' '.join(para))); para = []
    while i < len(lines):
        l = lines[i]
        if l.startswith('```'):
            flush(); i += 1; code = []
            while i < len(lines) and not lines[i].startswith('```'): code.append(lines[i]); i += 1
            out.append('<pre>%s</pre>' % html.escape('\n'.join(code))); i += 1; continue
        m = re.match(r'(#+)\s+(.*)', l)
        if m:
            flush(); lv = min(len(m.group(1)) + 1, 4); out.append('<h%d>%s</h%d>' % (lv, inline(m.group(2)), lv)); i += 1; continue
        if l.startswith('|') and i + 1 < len(lines) and re.match(r'\|[\s:|-]+\|?$', lines[i+1]):
            flush(); rows = [l]; i += 2
            while i < len(lines) and lines[i].startswith('|'): rows.append(lines[i]); i += 1
            out.append('<table border="1">')
            for n, r in enumerate(rows):
                cells = [c.strip() for c in r.strip().strip('|').split('|')]
                tag = 'th' if n == 0 else 'td'
                out.append('<tr>' + ''.join('<%s>%s</%s>' % (tag, inline(c), tag) for c in cells) + '</tr>')
            out.append('</table>'); continue
        m = re.match(r'\s*([-*]|\d+\.)\s+(.*)', l)
        if m:
            flush(); kind = 'ol' if m.group(1)[0].isdigit() else 'ul'; items = []
            while i < len(lines):
                m = re.match(r'\s*([-*]|\d+\.)\s+(.*)', lines[i])
                if m: items.append(m.group(2)); i += 1
                elif lines[i].startswith('  ') and lines[i].strip() and items: items[-1] += ' ' + lines[i].strip(); i += 1
                else: break
            out.append('<%s>%s</%s>' % (kind, ''.join('<li>%s</li>' % inline(x) for x in items), kind)); continue
        if not l.strip(): flush(); i += 1; continue
        if l.startswith('>'): l = l.lstrip('> ')
        para.append(l.strip()); i += 1
    flush()
out.append('</body></html>')
print('\n'.join(out))
