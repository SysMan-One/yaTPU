# Scenario tests: drive the editor in a pty.  usage: LANG=C.UTF-8 python3 tests/ptytest.py <path-to-tpu> <scratch-dir>
import os, pty, select, signal, struct, fcntl, termios, subprocess, sys, time, re
TPU = sys.argv[1]; W = sys.argv[2]
KEYCODE = os.path.join(os.path.dirname(TPU), 'keycode')

def spawn(args, rows=24, cols=80):
    m, s = pty.openpty()
    fcntl.ioctl(s, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
    p = subprocess.Popen([TPU] + args, stdin=s, stdout=s, stderr=s, cwd=W, close_fds=True)
    os.close(s)
    ACC.pop(m, None)
    return p, m

ACC = {}
def drain(m, t=0.4):
    out = b''
    end = time.time() + t
    while True:
        r, _, _ = select.select([m], [], [], max(0, end - time.time()))
        if not r: break
        try: d = os.read(m, 65536)
        except OSError: break
        if not d: break
        out += d
    ACC[m] = ACC.get(m, b'') + out
    return ACC[m]

def send(m, s, t=0.25):
    os.write(m, s if isinstance(s, bytes) else s.encode())
    return drain(m, t)

def finish(p, m, t=3):
    try: p.wait(timeout=t)
    except subprocess.TimeoutExpired: p.kill(); p.wait(); return None
    os.close(m); return p.returncode

def screen(raw, rows=24, cols=80):
    """crude VT100 interpreter: enough to see the text the editor drew"""
    scr = [[' ']*cols for _ in range(rows)]; r = c = 0; i = 0
    txt = raw.decode('latin1')
    while i < len(txt):
        ch = txt[i]
        if ch == '\x1b':
            mm = re.match(r'\x1b\[(\d*)(?:;(\d*))?([A-Za-z])', txt[i:])
            if mm:
                a, b, f = mm.groups(); i += mm.end()
                if f == 'H': r = (int(a or 1) - 1); c = (int(b or 1) - 1)
                elif f == 'J' and a == '2': scr = [[' ']*cols for _ in range(rows)]
                elif f == 'K': scr[r] = [' ']*cols
                continue
            i += 2; continue
        if ch == '\r': c = 0
        elif ch == '\n': r = min(r+1, rows-1)
        elif 0 <= r < rows and 0 <= c < cols:
            scr[r][c] = ch; c += 1
        i += 1
    return '\n'.join(''.join(l).rstrip() for l in scr)

def check(name, cond, extra=''):
    print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else '  ' + extra))

CZ = '\x1a'; CR = '\r'; DO = '\x1bOw'; F6 = '\x1b[17~'; CK = '\x0b'; CRr = '\x12'
SEL = '\x1b[4~'; REM = '\x1b[3~'; INS = '\x1b[2~'; RIGHT = '\x1b[C'; HOME = '\x1b[H'; END = '\x1b[F'
GOLD = '\x1bOo'

# 1. type, EXIT writes the file, journal removed
f = os.path.join(W, 't1.txt'); open(f, 'w').close()
p, m = spawn(['t1.txt']); out = drain(m)
check('1 status line', 'Buffer: t1.txt' in screen(out), screen(out)[-300:])
send(m, 'hello world' + CR + 'second'); out = send(m, CZ, 0.5); rc = finish(p, m)
check('1 exit rc=0', rc == 0, str(rc))
check('1 exit message', b'%TPU-S-WRITTEN, 2 lines written to file t1.txt' in out, repr(out[-120:]))
check('1 file content', open(f).read() == 'hello world\nsecond\n', repr(open(f).read()))
check('1 journal removed', not os.path.exists(f + '.tjl'))

# 2. LEARN "ab<CR>" -> F6, replay F6 twice, GOLD-3-F6 repeats three times
f = os.path.join(W, 't2.txt'); open(f, 'w').close()
p, m = spawn(['t2.txt']); drain(m)
send(m, CK); out = send(m, 'ab' + CR); send(m, CRr); out = send(m, F6)
check('2 remember msg', 'defined as a learned sequence' in screen(out), screen(out)[-200:])
send(m, F6); send(m, F6); send(m, GOLD + '3' + F6)
send(m, CZ); finish(p, m)
check('2 macro replay', open(f).read() == 'ab\n' * 6, repr(open(f).read()))

# 3. select / remove / insert here
f = os.path.join(W, 't3.txt'); open(f, 'w').close()
p, m = spawn(['t3.txt']); drain(m)
send(m, 'abcdef' + HOME + SEL + RIGHT + RIGHT + RIGHT + REM + END + INS)
send(m, CZ); finish(p, m)
check('3 cut/paste', open(f).read() == 'defabc\n', repr(open(f).read()))

# 4. DO command: SHOW BUFFERS, then BUFFER scratch, HELP LEARN
p, m = spawn([]); drain(m)
out = send(m, DO + 'sh b' + CR)
check('4 show buffers', 'MESSAGES' in screen(out) and 'INSERT HERE' in screen(out), screen(out)[:600])
send(m, CR)
out = send(m, DO + 'buf scratch' + CR)
check('4 scratch buffer', 'Buffer: scratch' in screen(out) and 'created' in screen(out), screen(out)[-300:])
out = send(m, DO + 'help learn' + CR); sc = screen(out)
check('4 help topic', sc.startswith('LEARN\n') and 'Topic?' in sc and '1 REMEMBER' not in sc, sc[:300])
out = send(m, CR); sc = screen(out)                                # Return: back to the list of topics, in columns
check('4 help list', 'Additional information available:' in sc and re.search(r'BUFFER\s+CHANGE CASE\s+CHANGE MODE', sc), sc[:600])
out = send(m, 'wild' + CR); sc = screen(out)                        # an abbreviation at the Topic? prompt
check('4 help abbrev', sc.startswith('WILDCARD FIND\n'), sc[:200])
out = send(m, CR); out = send(m, 'x' + CR); sc = screen(out)        # list, then an unknown topic
check('4 help sorry', sc.startswith('Sorry, no documentation on x') and 'Additional information' in sc, sc[:300])
send(m, CR)
out = send(m, DO + 'bogus' + CR)
check('4 unknown cmd', 'UNKCMD, unknown command' in screen(out), screen(out)[-200:])
out = send(m, 'x' + CZ)      # modified scratch buffer without a file: asks
check('4 exit asks', 'Write buffer scratch' in screen(out), screen(out)[-200:])
send(m, 'n' + CR); rc = finish(p, m)
check('4 exit after no', rc == 0, str(rc))

# 5. journal + recover after kill -9
f = os.path.join(W, 't5.txt'); open(f, 'w').close()
p, m = spawn(['t5.txt']); drain(m)
send(m, 'xyz' + CR + 'qq'); p.kill(); p.wait(); os.close(m)
check('5 journal kept', os.path.exists(f + '.tjl'))
p, m = spawn(['-recover', 't5.txt']); out = drain(m, 1.0)
check('5 recovered text', 'xyz' in screen(out) and 'qq' in screen(out), screen(out)[:300])
send(m, CZ); finish(p, m)
check('5 recovered file', open(f).read() == 'xyz\nqq\n', repr(open(f).read()))

# 6. resize: 24x80 -> 40x100, status line must move to row 38 (hint line 39, message line 40)
p, m = spawn([]); drain(m)
fcntl.ioctl(m, termios.TIOCSWINSZ, struct.pack('HHHH', 40, 100, 0, 0))
p.send_signal(signal.SIGWINCH); out = drain(m, 0.6)
sc = screen(out, 40, 100).split('\n')
check('6 resize status row', 'Buffer: MAIN' in sc[37] and 'EXIT' in sc[38], repr(sc[36:40]))
send(m, CZ); finish(p, m)

# 7. two windows, other window, one window
p, m = spawn([]); drain(m)
out = send(m, DO + 'two' + CR); sc = screen(out).split('\n')
check('7 two windows', sum('Buffer: MAIN' in l for l in sc) == 2, '\n'.join(sc))
out = send(m, DO + 'one' + CR); sc = screen(out).split('\n')
check('7 one window', sum('Buffer: MAIN' in l for l in sc) == 1)
send(m, CZ); finish(p, m)

# 8. replace all
f = os.path.join(W, 't8.txt'); open(f, 'w').write('foo bar foo\nfoo\n')
p, m = spawn(['t8.txt']); drain(m)
out = send(m, DO + 'replace foo' + CR + 'X' + CR); out = send(m, 'a')
check('8 replace count', '3 replacements' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)
check('8 replace file', open(f).read() == 'X bar X\nX\n', repr(open(f).read()))

# 9. UTF-8: cursor moves over whole characters, Delete erases one character
f = os.path.join(W, 't9.txt'); open(f, 'w').close()
p, m = spawn(['t9.txt']); drain(m)
out = send(m, 'привет мир' + '\x1b[D' * 3)
check('9 cursor column', out.endswith(b'\x1b[1;8H'), repr(out[-40:]))
send(m, '\x7f'); send(m, CZ); finish(p, m)
check('9 utf8 delete', open(f, encoding='utf-8').read() == 'приветмир\n', repr(open(f, encoding='utf-8').read()))

# 10. prompt recall: Up arrow brings back an earlier command
p, m = spawn([]); drain(m)
send(m, DO + 'buffer zz' + CR); send(m, DO + 'buffer MAIN' + CR)
out = send(m, DO + '\x1b[A' + '\x1b[A' + CR)
check('10 recall', 'Buffer: zz' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)

# 11. wildcard find on whole lines
f = os.path.join(W, 't11.txt'); open(f, 'w').write('aaa\nfoo bar\nbaz\n')
p, m = spawn(['t11.txt']); drain(m)
out = send(m, DO + 'wild *bar' + CR)
check('11 wildcard find', 'Line 2 of 4' in screen(out), screen(out)[-200:])
out = send(m, DO + 'wild *zzz*' + CR)
check('11 wildcard miss', 'could not find' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)

# 12. case-insensitive search and replace in Cyrillic
f = os.path.join(W, 't12.txt'); open(f, 'w', encoding='utf-8').write('Привет МИР\nмир\n')
p, m = spawn(['t12.txt']); drain(m)
out = send(m, DO + 'find мир' + CR)
check('12 ci find cursor', out.endswith(b'\x1b[1;8H'), repr(out[-40:]))
out = send(m, DO + 'find Мир' + CR)
check('12 cs find misses', 'could not find' in screen(out), screen(out)[-200:])
send(m, DO + 'top' + CR)
out = send(m, DO + 'replace мир' + CR + 'свет' + CR); out = send(m, 'a')
check('12 ci replace count', '2 replacements' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)
check('12 ci replace file', open(f, encoding='utf-8').read() == 'Привет свет\nсвет\n', repr(open(f, encoding='utf-8').read()))

# 13. GET FILE switches to a buffer named after the file; INCLUDE FILE inserts at the cursor
f = os.path.join(W, 't13.txt'); open(f, 'w').close()
incf = os.path.join(W, 'inc.txt'); open(incf, 'w').write('included line\n')
p, m = spawn(['t13.txt']); drain(m)
send(m, DO + 'include inc.txt' + CR)
out = send(m, DO + 'get file inc.txt' + CR)
check('13 get file switches buffer', 'Buffer: inc.txt' in screen(out) and 'included line' in screen(out), screen(out)[:300])
send(m, DO + 'buffer t13.txt' + CR)
send(m, CZ); finish(p, m)
check('13 include file content', open(f).read() == 'included line\n', repr(open(f).read()))

# 14. WRITE FILE to a different name: buffer's file changes, the old file is untouched
f = os.path.join(W, 't14.txt'); open(f, 'w').write('line one\n')
newf = os.path.join(W, 't14-renamed.txt')
p, m = spawn(['t14.txt']); drain(m)
send(m, 'modified ')
out = send(m, DO + 'write file ' + newf + CR)
check('14 write file message', '1 lines written to file' in screen(out), screen(out)[-200:])
check('14 new file content', open(newf).read() == 'modified line one\n', repr(open(newf).read()))
check('14 old file untouched', open(f).read() == 'line one\n', repr(open(f).read()))
send(m, CZ); finish(p, m)

# 15. STORE TEXT copies (does not cut) the selection; RESTORE brings back an erased word
f = os.path.join(W, 't15.txt'); open(f, 'w').close()
KPMINUS = '\x1bOm'
p, m = spawn(['t15.txt']); drain(m)
out = send(m, 'abcdef' + HOME + SEL + RIGHT * 3 + GOLD + REM)
check('15 store text message', 'copied to the INSERT HERE buffer' in screen(out), screen(out)[-200:])
send(m, END + INS)
send(m, CR + 'hello world' + HOME + '\x0a' + GOLD + KPMINUS)          # GOLD-KP- RESTORE
send(m, CZ); finish(p, m)
check('15 store/restore file', open(f).read() == 'abcdefabc\nhello world\n', repr(open(f).read()))

# 16. DEFINE KEY rebinds a key; the new binding takes effect immediately
f = os.path.join(W, 't16.txt'); open(f, 'w').close()
p, m = spawn(['t16.txt']); drain(m)
send(m, 'line1' + CR + 'line2' + CR + 'line3')
out = send(m, DO + 'define key TOP' + CR)
check('16 define key prompt', 'Press the key to define' in screen(out), screen(out)[-200:])
out = send(m, F6)
check('16 define key defined', 'F6' in screen(out) and 'TOP' in screen(out), screen(out)[-200:])
send(m, F6); send(m, 'X')
send(m, CZ); finish(p, m)
check('16 define key effect', open(f).read() == 'Xline1\nline2\nline3\n', repr(open(f).read()))

# 17. NEXT SCREEN on a short buffer keeps the text on the screen; PREVIOUS SCREEN comes back
f = os.path.join(W, 't17.txt'); open(f, 'w').write(''.join('line %d\n' % i for i in range(1, 11)))
p, m = spawn(['t17.txt']); drain(m)
out = send(m, '\x1b[6~'); sc = screen(out)
check('17 next screen keeps text', 'line 1' in sc and 'Line 11 of 11' in sc, sc[:300])
out = send(m, '\x1b[5~'); sc = screen(out)
check('17 prev screen', 'Line 1 of 11' in sc, sc[-200:])
send(m, CZ); finish(p, m)

# 18. SHOW KEYS on GOLD-K and GOLD-F1 lists the live table, DEFINE KEY shows up in it
p, m = spawn([]); drain(m)
out = send(m, GOLD + 'k'); sc = screen(out)
check('18 gold-k show keys', sc.startswith('Key bindings:') and re.search(r'F1\s+HELP\s+KP/\s+GOLD', sc), sc[:400])
out = send(m, GOLD + 'f' + 'GOLD-k' + CR); sc = screen(out)      # FIND works inside the listing
check('18 gold-k listed', re.search(r'GOLD-k\s+SHOW KEYS', sc) is not None, sc[:600])
send(m, CR)
send(m, DO + 'define key TOP' + CR); send(m, F6)
out = send(m, GOLD + '\x1bOP' + GOLD + '\x1b[B'); sc = screen(out)  # GOLD-F1, then GOLD-Down: BOTTOM
check('18 gold-f1 after define', re.search(r'F6\s+TOP', sc) is not None, sc[:600])
send(m, CR); send(m, CZ); finish(p, m)

# 19. key hint line: nano style, follows DEFINE KEY, HINTS hides it and gives the row back to the text
p, m = spawn([]); drain(m)
out = drain(m); sc = screen(out).split('\n')
check('19 hint line', sc[22].startswith('KP- DO  ^Z EXIT  F1 HELP  GOLD-k SHOW KEYS'), repr(sc[22]))
out = send(m, DO + 'define key HELP' + CR); out = send(m, '\x1bOm'); sc = screen(out).split('\n')   # KP- taken: DO falls back to KP7
check('19 hint follows define key', sc[22].startswith('KP7 DO  ^Z EXIT  F1 HELP'), repr(sc[22]))
check('19 hint keys highlighted', b'\x1b[7mKP7\x1b[0m DO' in out, repr(out[-400:]))
out = send(m, DO + 'hints' + CR); sc = screen(out).split('\n')   # KP7 is still DO
check('19 hints off', 'Buffer: MAIN' in sc[22] and 'HINTSOFF' in sc[23], repr(sc[21:24]))
send(m, CZ); finish(p, m)

# 20. keycode: known sequence, unknown sequence, UTF-8 character, Ctrl-C leaves
m, s = pty.openpty()
p = subprocess.Popen([KEYCODE], stdin=s, stdout=s, stderr=s, cwd=W, close_fds=True); os.close(s); ACC.pop(m, None); drain(m)
for k in ('\x1bOS', '\x1b[99~', 'п', '\x03'): out = send(m, k, 0.3)      # one key at a time, as typed
out = out.decode('utf-8', 'replace')
check('20 keycode known', re.search(r'1b 4f 53\s+ESC O S\s+F4', out) is not None, out[-300:])
check('20 keycode unknown', 'unknown to TPU' in out, out[-300:])
check('20 keycode utf8', 'UTF-8 character' in out and finish(p, m) == 0, out[-300:])

# 21. PC keypad map: Esc is GOLD, Begin (keypad 5) selects, GOLD-Enter is DO, GOLD-Left/Right step words,
#     GOLD-minus sets REVERSE, GOLD-Home goes to the top
f = os.path.join(W, 't21.txt'); open(f, 'w').close()
p, m = spawn(['t21.txt']); drain(m)
ESC = '\x1b'; BEGIN = '\x1b[E'; LEFT = '\x1b[D'
send(m, 'one two three' + HOME)
send(m, BEGIN); send(m, RIGHT * 3); send(m, REM); send(m, END); send(m, INS)          # cut "one", paste at the end
out = send(m, ESC, 0.3); out = send(m, CR + 'buffer x' + CR); sc = screen(out)          # GOLD-Enter: DO
check('21 gold-enter do', 'Buffer: x' in sc, sc[-300:])
send(m, ESC, 0.3); send(m, CR + 'buffer t21.txt' + CR)
send(m, ESC, 0.3); out = send(m, HOME); sc = screen(out)                                # GOLD-Home: TOP
check('21 gold-home top', 'Line 1 of 1' in sc, sc[-200:])
send(m, ESC, 0.3); out = send(m, RIGHT); send(m, ESC, 0.3); out = send(m, RIGHT)          # two words forward
send(m, 'X')
send(m, ESC, 0.3); out = send(m, '-'); sc = screen(out)                                  # GOLD-minus: REVERSE
check('21 gold-minus reverse', 'Reverse' in sc, sc[-300:])
send(m, ESC, 0.3); send(m, '\x1b[D'); send(m, 'Y')                                      # GOLD-Left: word back, even in reverse
out = send(m, ESC, 0.3); out = send(m, '*'); sc = screen(out)                            # GOLD-*: SHOW KEYS
check('21 gold-star show keys', sc.startswith('Key bindings:'), sc[:200])
send(m, CR); send(m, CZ); finish(p, m)
check('21 pc keypad file', open(f).read() == ' Ytwo Xthreeone\n', repr(open(f).read()))   # words: "two", "threeone"

# 22. the selection is drawn in reverse video, across lines the newline is one highlighted cell
f = os.path.join(W, 't22.txt'); open(f, 'w').write('abcdef\nghij\n')
p, m = spawn(['t22.txt']); drain(m)
send(m, RIGHT); out = send(m, SEL); out = send(m, RIGHT * 3)
check('22 selection highlighted', b'\x1b[7mbcd\x1b[0m' in out, repr(out[-200:]))
out = send(m, '\x1b[B'); out = send(m, '\x1b[B')          # Down twice: the whole rest of line 1, then "ghij" up to column 4
check('22 multi-line', b'\x1b[7mbcdef \x1b[0m' in out and b'\x1b[7mghij\x1b[0m' in out, repr(out[-400:]))
send(m, CZ); finish(p, m)

# 23. key files: SAVE KEYS writes the table with a macro, a hand-edited file loads with NONE and a bad line,
#     -keys reads a file at start
kf = 'k23.keys'; f = os.path.join(W, 't23.txt'); open(f, 'w').close()     # relative: the editor runs in W, messages stay short
F7 = '\x1b[18~'; F8 = '\x1b[19~'
p, m = spawn(['t23.txt']); drain(m)
send(m, CK); send(m, 'ab' + CR); send(m, CRr); send(m, F6)                   # F6 = "ab<Return>"
out = send(m, DO + 'save keys ' + kf + CR)
check('23 save keys', 'key definitions written to' in screen(out), screen(out)[-200:])
txt = open(os.path.join(W, kf)).read()
check('23 file has gold line', re.search(r'^KP/, Escape\s+= GOLD$', txt, re.M) is not None, txt[:400])
check('23 file has macro', re.search(r'^F6\s+= KEYS "ab\{Return\}"$', txt, re.M) is not None, txt[-300:])
send(m, CZ); finish(p, m)
open(os.path.join(W, kf), 'a').write('F7, GOLD-KP,  = TOP\nF8 = KEYS "x{Return}"   ! macro\nGOLD-k = NONE\nF9 = BOGUS\n')
open(f, 'w').close()                                                       # the LEARN above typed into it
p, m = spawn(['t23.txt']); drain(m)
out = send(m, DO + 'load keys ' + kf + CR)
check('23 load reports bad line', '1 bad lines' in screen(out), screen(out)[-200:])
send(m, 'one' + CR + 'two'); send(m, F7); send(m, F8); send(m, '\x1b[17~')   # F7 TOP, F8 "x<CR>", F6 "ab<CR>"
out = send(m, GOLD + 'k')
check('23 none unbinds', 'KEYUNDEF' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)
check('23 loaded macros work', open(f).read() == 'x\nab\none\ntwo\n', repr(open(f).read()))
p, m = spawn(['-keys', kf, 't23.txt']); out = drain(m, 0.6)
check('23 -keys at start', '1 bad lines' in screen(out), screen(out)[-200:])
send(m, CZ); finish(p, m)

# 24. EDT keypad: KP3 MOVE BY CHAR follows the direction (KP4 / KP5), KP9 ERASE LINE, GOLD-KP- RESTORE
f = os.path.join(W, 't24.txt'); open(f, 'w').close()
KP3 = '\x1bOs'; KP4 = '\x1bOt'; KP5 = '\x1bOu'; KP9 = '\x1bOy'; KPMINUS = '\x1bOm'
p, m = spawn(['t24.txt']); drain(m)
send(m, 'abc' + CR + 'def' + CR + 'ghi' + GOLD + KP5)             # GOLD-KP5 TOP
send(m, KP3 + KP3 + 'X' + KP5 + KP3 + 'Y' + KP4)                # -> abXc, back one: abYXc
send(m, '\x1b[B' + KP9); send(m, CZ); finish(p, m)
check('24 move by char, erase line', open(f).read() == 'abYXc\nghi\n', repr(open(f).read()))
p, m = spawn(['t24.txt']); drain(m)
send(m, '\x1b[B' + KP9 + GOLD + KPMINUS); send(m, CZ); finish(p, m)
check('24 restore line', open(f).read() == 'abYXc\nghi\n', repr(open(f).read()))

# 25. EDT's LINE, OPEN LINE, CHNGCASE, DEL EOL, as EDT on VMS does them: cursor on "f" of "five", the key, "X"
KP0 = '\x1bOp'; KP1 = '\x1bOq'; KP2 = '\x1bOr'
START = 'one two three' + CR + 'four five six' + CR + 'seven eight nine' + GOLD + KP5 + '\x1b[B' + RIGHT * 5
for name, keys, want in (('KP0 next line start', KP0, 'one two three\nfour five six\nXseven eight nine\n'),
                         ('KP0 reverse', KP5 + KP0, 'one two three\nXfour five six\nseven eight nine\n'),
                         ('GOLD-KP0 open line', GOLD + KP0, 'one two three\nfour X\nfive six\nseven eight nine\n'),
                         ('GOLD-KP1 change case', GOLD + KP1, 'one two three\nfour FXive six\nseven eight nine\n'),
                         ('GOLD-KP2 erase to end', GOLD + KP2, 'one two three\nfour X\nseven eight nine\n'),
                         ('GOLD-KP2 then RESTORE', GOLD + KP2 + GOLD + KPMINUS, 'one two three\nfour five sixX\nseven eight nine\n')):
    f = os.path.join(W, 't25.txt'); open(f, 'w').close()
    p, m = spawn(['t25.txt']); drain(m)
    send(m, START); send(m, keys); send(m, 'X'); send(m, CZ); finish(p, m)
    check('25 ' + name, open(f).read() == want, repr(open(f).read()))
f = os.path.join(W, 't25.txt'); open(f, 'w').close()
p, m = spawn(['t25.txt']); drain(m)
send(m, 'мир Hello' + HOME + SEL + '\x1b[F' + GOLD + KP1); send(m, CZ); finish(p, m)
check('25 change case selection', open(f).read() == 'МИР hELLO\n', repr(open(f).read()))
