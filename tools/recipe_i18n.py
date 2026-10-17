"""範例內容（清單上的名稱、圖上的字）的多國語系。

  python tools/recipe_i18n.py update
      範例改了文字之後跑這個：重新抽出所有要翻的字串，更新 demo/i18n/recipes_*.ts。
      已有的翻譯保留；新字串標成 unfinished，用 Qt Linguist 打開 .ts 補上即可；不再用到的字串刪掉。

  python tools/recipe_i18n.py import <語言> <翻譯.json>
      匯入一批翻譯（JSON：{"原文": "譯文", ...}），寫進 demo/i18n/recipes_<語言>.ts。

  python tools/recipe_i18n.py transform demo
      把範例裡的 QStringLiteral("中文")／QString::fromUtf8(...) 改成 RT(...)。已經做過，新範例直接寫 RT("…")。

範例的說明段落不翻，所以不會被抽出來。
"""
import json
import re
import sys

STR = r'"(?:[^"\\]|\\.)*"'
CJK = re.compile('[　-ヿ一-鿿＀-￯]')


def c_unescape(lit):
    """一個 C 字串常值（含引號）→ 實際文字。"""
    body = lit[1:-1]
    out, i = [], 0
    while i < len(body):
        ch = body[i]
        if ch == '\\' and i + 1 < len(body):
            nx = body[i + 1]
            out.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'"}.get(nx, '\\' + nx))
            i += 2
        else:
            out.append(ch)
            i += 1
    return ''.join(out)


def join_literals(seq):
    return ''.join(c_unescape(m) for m in re.findall(STR, seq))


def strip_comments(code):
    # 拿掉 // 註解，但不能拿掉字串裡的 //（網址）
    out = []
    for line in code.split('\n'):
        res, i, in_str = [], 0, False
        while i < len(line):
            ch = line[i]
            if in_str:
                res.append(ch)
                if ch == '\\' and i + 1 < len(line):
                    res.append(line[i + 1])
                    i += 2
                    continue
                if ch == '"':
                    in_str = False
            else:
                if line.startswith('//', i):
                    break
                if ch == '"':
                    in_str = True
                res.append(ch)
            i += 1
        out.append(''.join(res))
    return '\n'.join(out)


def transform(demo):
    import glob
    n_lit = n_utf = 0
    for f in sorted(glob.glob(demo + '/recipes/*.cpp')):
        s = open(f, encoding='utf-8').read()
        orig = s

        def rep(m):
            nonlocal n_lit
            if not CJK.search(join_literals(m.group(1))):
                return m.group(0)
            n_lit += 1
            return 'RT(' + m.group(1).strip() + ')'

        s = re.sub(r'QStringLiteral\(\s*((?:' + STR + r'\s*)+)\)', rep, s)
        c = s.count('QString::fromUtf8(')
        n_utf += c
        s = s.replace('QString::fromUtf8(', 'RT(')
        if s != orig:
            open(f, 'w', encoding='utf-8').write(s)
    print('QStringLiteral → RT:', n_lit, '  fromUtf8 → RT:', n_utf)


def extract(demo, out_path):
    import glob
    head = re.compile(r'QTPLOT_RECIPE\w*\(\s*(' + STR + r')\s*,\s*(' + STR + r')\s*,\s*(' + STR + r')\s*,((?:\s*' + STR + r')+)')
    tree, inplot = [], []
    seen = set()

    def add(lst, text, kind):
        if text and CJK.search(text) and text not in seen:
            seen.add(text)
            lst.append({'kind': kind, 'zh': text})

    for f in sorted(glob.glob(demo + '/recipes/*.cpp')):
        s = strip_comments(open(f, encoding='utf-8').read())
        for m in head.finditer(s):
            add(tree, c_unescape(m.group(1)), 'chapter')
            add(tree, c_unescape(m.group(2)), 'category')
            add(tree, c_unescape(m.group(3)), 'recipe name')
        body = head.sub('', s)  # 說明不翻
        # 相鄰的字串常值（中間只有空白）是同一個字串
        for m in re.finditer(r'(?:' + STR + r'\s*)+', body):
            add(inplot, join_literals(m.group(0)), 'text drawn on the plot')
    items = tree + inplot
    json.dump(items, open(out_path, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    print('tree:', len(tree), ' on-plot:', len(inplot), ' total:', len(items))


LANGS = ['en', 'zh_CN', 'ja', 'ko', 'fr', 'de', 'it', 'es']
ROOT = __import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__)))


def ts_path(lang):
    return ROOT + '/demo/i18n/recipes_%s.ts' % lang


def read_ts(lang):
    """回傳 {原文: 譯文}（只收已翻好的）。"""
    import html
    import os
    path = ts_path(lang)
    if not os.path.exists(path):
        return {}
    s = open(path, encoding='utf-8').read()
    out = {}
    for m in re.finditer(r'<message>(.*?)</message>', s, re.S):
        block = m.group(1)
        src = html.unescape(re.search(r'<source>(.*?)</source>', block, re.S).group(1))
        tr = re.search(r'<translation([^>]*)>(.*?)</translation>', block, re.S)
        if tr and 'unfinished' not in tr.group(1) and tr.group(2):
            out[src] = html.unescape(tr.group(2))
    return out


def write_ts(lang, sources, translations):
    from xml.sax.saxutils import escape
    lines = ['<?xml version="1.0" encoding="utf-8"?>', '<!DOCTYPE TS>',
             '<TS version="2.1" language="%s" sourcelanguage="zh_TW">' % lang, '<context>', '    <name>recipe</name>']
    missing = 0
    for src in sources:
        tr = translations.get(src)
        lines.append('    <message>')
        lines.append('        <source>%s</source>' % escape(src))
        if tr:
            lines.append('        <translation>%s</translation>' % escape(tr))
        else:
            missing += 1
            lines.append('        <translation type="unfinished"></translation>')
        lines.append('    </message>')
    lines += ['</context>', '</TS>', '']
    open(ts_path(lang), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    return missing


def all_sources():
    import tempfile
    import os
    tmp = os.path.join(tempfile.gettempdir(), 'recipe_strings.json')
    extract(ROOT + '/demo', tmp)
    return [it['zh'] for it in json.load(open(tmp, encoding='utf-8'))]


if __name__ == '__main__':
    if sys.argv[1] == 'update':
        sources = all_sources()
        for lang in LANGS:
            print(lang, '未翻:', write_ts(lang, sources, read_ts(lang)))
        sys.exit(0)
    if sys.argv[1] == 'import':
        lang, mapping = sys.argv[2], json.load(open(sys.argv[3], encoding='utf-8'))
        merged = read_ts(lang)
        merged.update(mapping)
        print(lang, '未翻:', write_ts(lang, all_sources(), merged))
        sys.exit(0)
    if sys.argv[1] == 'transform':
        transform(sys.argv[2])
    elif sys.argv[1] == 'extract':
        extract(sys.argv[2], sys.argv[3])
