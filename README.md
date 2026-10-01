# TPU

A lightweight text editor in the spirit of DEC TPU/EVE. Plain C, termios and VT100
escape sequences, no ncurses. Depends only on the installed StarLet package (`utility_routines`, 1.6.3 or later, found via `find_package(StarLet)`).
The key table `tpukeys.h` is shared with `keycode`, a utility that prints the bytes a key sends and the TPU name of the key.

## Build

    cmake -S TPU -B build-tpu && cmake --build build-tpu      # standalone
    cmake -S . -B build && cmake --build build --target tpu   # from the WorkBench tree

    cmake --install build-tpu --prefix /usr/local                  # tpu, keycode -> bin; docs -> share/doc/tpu

    ./build-tpu/tpu [-recover] [-nojournal] [-keys file] [file]
    ./build-tpu/keycode [-normal]      # what does the terminal send for a key, and what TPU calls it

## Ideas borrowed from TPU/EVE

* **Buffers, not files.** A buffer is the central object; a file is an optional attribute.
  `BUFFER name` gives a scratch buffer tied to nothing. The paste buffer `INSERT HERE`,
  the `MESSAGES` log, `$RESTORE` and `HELP` are ordinary buffers: `SHOW BUFFERS` lists them all.
* **One input path.** Every key goes through `s_key_get()`. `LEARN` records raw keys,
  `REMEMBER` (Ctrl-R) binds them to a key, `REPEAT n` or GOLD-digits replays; the journal
  `<file>.tjl` is the same key stream and `-recover` replays it after a crash. A failing command
  stops any replay, so `REPEAT 999 <macro>` walks through a buffer until FIND fails.
* **DO prompt** (KP-, KP7, GOLD-Enter) with abbreviations: `GET F foo.c`, `SH B`, `TWO`.
  Up/Down recall earlier answers.
* **Windows follow the terminal.** Size comes from `TIOCGWINSZ`/`SIGWINCH`, with a DSR/CPR
  query as fallback for a real VT on a serial line. `TWO WINDOWS`, `OTHER WINDOW`, `ONE WINDOW`.
* **Help** is `tpu.hlp` in VMS `.HLP` layout, embedded at build time; `HELP KEYS` is generated
  from the live key table.
* **Key hint line** above the message line, nano style: the key in reverse video, the command next
  to it, as many as fit the width, taken from the live key table. `HINTS` hides and shows it.

Text is bytes; UTF-8 is understood where it matters: the cursor moves over whole characters,
display columns use `wcwidth()`, and a search pattern without upper-case letters matches
regardless of case in any script the locale knows. TAB is expanded on display only. There is
no undo, only `RESTORE`.

Messages are VMS condition messages, `%TPU-E-NOTFOUND, could not find: foo`. The catalogue is
`tpumsg.h`, one line per message; `tpu.c` turns it into `TPU$_NAME` condition codes (facility,
number, severity) and registers it with StarLet's `__util$inimsg()`. A command returns its
condition code, and only E and F severities stop a replayed key sequence.

## User guides

[GUIDE-EN.md](GUIDE-EN.md) (English) and [GUIDE-RU.md](GUIDE-RU.md) (Russian): a beginner's walk through the
editor, every message code with what to do about it, and a troubleshooting section.

## Keys (defaults)

See [`KEYS.txt`](KEYS.txt) for the full table laid out on a physical PC/xterm keyboard, `HELP KEYPAD`
draws the keypad. The keypad follows EDT emulators on a PC, which put the VT's PF1-PF4 on NumLock / * -:
NumLock never reaches a terminal program, so GOLD sits on / and HELP on F1, the only function key bound.
Short form:

    F1   HELP (GOLD: SHOW KEYS)
    KP/  GOLD                  KP*  FIND NEXT          KP-  DO                 KP+  ERASE CHARACTER
    KP7  DO                    KP8  NEXT SCREEN        KP9  ERASE LINE         KP,  ERASE CHARACTER (VT)
    KP4  FORWARD               KP5  REVERSE            KP6  REMOVE             Enter RETURN
    KP1  MOVE BY WORD          KP2  END OF LINE        KP3  MOVE BY CHAR       KP0  MOVE BY LINE
    KP.  SELECT
    Insert INSERT HERE   GOLD-KP4 BOTTOM   GOLD-KP5 TOP   GOLD-KP6 INSERT HERE   GOLD-KP8 PREVIOUS SCREEN   GOLD-KP* FIND
    GOLD-KP- / GOLD-KP+ RESTORE   GOLD-Enter DO
    Find, Insert Here, Remove, Select, Prev/Next Screen, Home, End as labelled
    PC keypad without application mode: Esc GOLD, 5 SELECT, GOLD-Enter DO, GOLD-Home/End TOP/BOTTOM,
    GOLD-Left/Right PREVIOUS/NEXT WORD, GOLD-minus/plus REVERSE/FORWARD, GOLD-/ HELP, GOLD-* SHOW KEYS
    Ctrl-J ERASE WORD   Ctrl-Z EXIT   Ctrl-K LEARN   Ctrl-R REMEMBER   Ctrl-W REFRESH
    Ctrl-A CHANGE MODE   Ctrl-U ERASE START OF LINE   Ctrl-E END OF LINE   Ctrl-H START OF LINE
    GOLD-letter: B BUFFER, D DEFINE KEY, F FIND, G GET FILE, H HELP, I INCLUDE FILE, K SHOW KEYS, L LINE,
                 O OTHER WINDOW, Q QUIT, R REPLACE, S SHOW BUFFERS, T TWO WINDOWS, W WRITE FILE,
                 X EXIT, Z ONE WINDOW

`DEFINE KEY` rebinds anything, F2-F12 included; `SHOW KEYS` (GOLD-K, GOLD-F1) or `HELP KEYS` shows the current table.
If / and * of the keypad type text, the terminal ignores application keypad mode: Esc is GOLD then,
and `keycode` shows what the keys send.

## Key files

`SAVE KEYS` writes the live table, learned sequences included, as `tpu.keys`; `LOAD KEYS` reads one back.
At start `~/.tpu.keys`, `./tpu.keys` and the file from `-keys` or `TPU_KEYS` are read in that order.

    KP/, Esc                   = GOLD
    GOLD-Left                  = PREVIOUS WORD
    F6                         = KEYS "ab{Return}{GOLD-KP1}"   ! a learned sequence
    GOLD-k                     = NONE                        ! unbinds

## Tests

    LANG=C.UTF-8 python3 TPU/tests/ptytest.py $PWD/build-tpu/tpu /tmp/tpu-w

Both paths must be absolute: the test starts the editor with the scratch directory as its working directory.

Drives the editor in a pseudo-terminal through 24 scenarios (72 checks): editing and EXIT, LEARN/REPEAT,
cut and paste, DO commands, kill -9 and `-recover`, SIGWINCH resize, two windows, REPLACE,
UTF-8, recall, WILDCARD FIND, case-insensitive Cyrillic search and replace, the EDT keypad.

    LANG=C.UTF-8 python3 TPU/tests/esctest.py $PWD/build-tpu/tpu /tmp/tpu-e [<yaVTERM build dir> | ssh | ssh:<host>]

Keys whose escape sequence arrives split in time, ESC first and the rest 30 to 200 ms later, as a tunnel or a slow
line delivers it: arrows in both forms, Home, End, Delete, KP-, and Esc as GOLD followed by a letter fast or slow;
then the keypad itself (KP- DO, KP9 ERASE LINE, KP3, KP/ GOLD, Insert pasting), whole and split.
Each result is compared with the same keys sent at once. Given a yaVTERM build directory (with `vtc` and `vtd`),
the split runs go through the tunnel, `vtc` -> `vtd` on a free local port -> the editor; given `ssh`, through
OpenSSH, `ssh -tt` -> a private `sshd` with its own keys on a free local port -> the editor; given `ssh:<host>`,
through the sshd already running there with your own keys (`ssh:localhost` needs your key in `authorized_keys`).
