#!/usr/bin/env python3
"""檢查每個圖元的字型欄位都有被 visitFonts() 走到。

為什麼需要這個檢查：Plot::visitFonts() 是「一次改掉整張圖所有文字」的唯一入口，
而它靠的是每個圖元自己把欄位交出來。少交一個，setFontFamily() 之類的捷徑就會
漏掉那一處 —— 而且漏掉的地方只有畫出來才看得見，沒有任何編譯錯誤。

C++ 沒有反射，所以只能從原始碼反查。這個腳本做的事：
  1. 掃 src/qtplot/*.h，找出每個 class 裡宣告的 FontStyle / Label 成員
     （包含繼承自 IPlottable 的圖元，以及 Axis、Legend 這種非圖元的）
  2. 掃 .h 與 .cpp 裡該 class 的 visitFonts() 實作，看每個欄位名是否出現
  3. 有漏的就印出來並以非 0 結束

用法：python tools/check_fonts.py
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'qtplot')

# 這些型別本身不含文字，或由別的機制處理，不列入檢查
SKIP_CLASSES = {
    'FontStyle',   # 就是字型本身
    'Label',       # 由持有它的 class 交出去
    'LegendStyleDialog',  # 對話框暫存的字型，按確定時才寫進 Legend.font（Legend 自己會交出去）
}

CLASS_RE = re.compile(r'^(?:class|struct)\s+(\w+)\s*(?::[^{]*)?\{', re.M)
FIELD_RE = re.compile(r'^\s*(FontStyle|Label)\s+(\w+)\s*(?:\{[^}]*\})?\s*;', re.M)


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def body_of(text, header_pattern):
    """抓出符合 header_pattern 的那個函式的本體（含巢狀括號）。沒有就回傳空字串。"""
    m = re.search(header_pattern, text)
    if not m:
        return ''
    depth = 0
    start = m.end() - 1
    for i in range(start, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return ''


def class_bodies(text):
    """回傳 {class 名稱: 大括號內的內容}。巢狀的 struct 會被併進外層，夠用了。"""
    out = {}
    for m in CLASS_RE.finditer(text):
        name = m.group(1)
        depth = 0
        start = m.end() - 1
        for i in range(start, len(text)):
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
                if depth == 0:
                    out[name] = text[start + 1:i]
                    break
    return out


def main():
    headers = sorted(f for f in os.listdir(ROOT) if f.endswith('.h'))
    sources = {f: strip_comments(read(os.path.join(ROOT, f)))
               for f in os.listdir(ROOT) if f.endswith('.cpp')}
    all_cpp = '\n'.join(sources.values())

    problems = []
    checked = 0
    for h in headers:
        text = strip_comments(read(os.path.join(ROOT, h)))
        bodies = class_bodies(text)
        for cls, body in bodies.items():
            if cls in SKIP_CLASSES:
                continue
            fields = [m.group(2) for m in FIELD_RE.finditer(body)]
            if not fields:
                continue
            checked += 1

            # visitFonts 可能寫在標頭的 inline（單行或多行），也可能寫在 .cpp。
            # 用括號配對而不是正規式抓函式本體 —— 正規式得假設收尾的縮排長什麼樣，
            # 單行寫法就會被漏掉，而漏掉的下場是「以為有實作」反過來變成假通過。
            impl = (body_of(body, r'void\s+visitFonts\s*\([^)]*\)[^{]*\{') +
                    body_of(all_cpp, r'void\s+' + cls + r'::visitFonts\s*\([^)]*\)[^{]*\{'))

            if not impl:
                problems.append('%s (%s)：完全沒有 visitFonts()，欄位 %s'
                                % (cls, h, ', '.join(fields)))
                continue
            missing = [f for f in fields if not re.search(r'\b' + f + r'\b', impl)]
            if missing:
                problems.append('%s (%s)：visitFonts() 漏掉 %s'
                                % (cls, h, ', '.join(missing)))

    print('檢查了 %d 個含文字欄位的型別' % checked)
    if problems:
        print('\n有 %d 個問題：' % len(problems))
        for p in problems:
            print('  ' + p)
        return 1
    print('全部的字型欄位都會被 visitFonts() 走到')
    return 0


if __name__ == '__main__':
    sys.exit(main())
