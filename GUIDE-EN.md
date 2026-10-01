# TPU. User Guide

Version X-00-10. Русская версия: [GUIDE-RU.md](GUIDE-RU.md).

TPU is a small text editor for the terminal. It is made after the TPU/EVE editor of
the OpenVMS system. If you have used EDT or EVE, you will feel at home. If not, this guide
tells everything from the start.

Contents:

1. [Build and start](#1-build-and-start)
2. [What you see on the screen](#2-what-you-see-on-the-screen)
3. [The GOLD key](#3-the-gold-key)
4. [Type and edit text](#4-type-and-edit-text)
5. [Move around](#5-move-around)
6. [Save and leave](#6-save-and-leave)
7. [Commands: the DO key](#7-commands-the-do-key)
8. [Buffers and files](#8-buffers-and-files)
9. [Select, cut, paste](#9-select-cut-paste)
10. [Find and replace](#10-find-and-replace)
11. [Two windows](#11-two-windows)
12. [Macros: LEARN, REPEAT, DEFINE KEY](#12-macros-learn-repeat-define-key)
13. [Journal and recovery after a crash](#13-journal-and-recovery-after-a-crash)
14. [Help inside the editor](#14-help-inside-the-editor)
15. [Messages: what they mean and what to do](#15-messages-what-they-mean-and-what-to-do)
16. [What to do if...](#16-what-to-do-if)
17. [Key table](#17-key-table)

## 1. Build and start

You need the StarLet package installed (`utility_routines`, version 1.6.3 or newer).
It usually lives in `/usr/local`.

    cmake -S . -B build
    cmake --build build

The programs are `build/tpu` (the editor) and `build/keycode` (a key checker, see section 16). Start it:

    ./build/tpu             # an empty buffer called MAIN
    ./build/tpu notes.txt   # open a file; if it does not exist, it is created when you write

Start options:

| Option       | What it does                                                      |
|--------------|-------------------------------------------------------------------|
| `-recover`   | Replay every key from the journal after a crash (see section 13)  |
| `-nojournal` | Do not keep a journal                                             |
| `-keys file` | Read the key bindings from a file (see section 12)                |
| `-version`   | Print the version and leave                                       |

For case-insensitive search in any language, start with a UTF-8 locale:

    LANG=C.UTF-8 ./build/tpu notes.txt

## 2. What you see on the screen

The screen has four parts:

    hello world                              <- the text
    second line
    [End of file]
     Buffer: notes.txt*     | Insert | Forward | Line 2 of 3     <- status line
    KP- DO  ^Z EXIT  F1 HELP  GOLD-k SHOW KEYS  Find FIND      <- key hints
    %TPU-S-READ, 2 lines read from file notes.txt              <- message line

The **status line** shows:

- the buffer name. A star `*` after the name means there are unsaved changes;
- the mode: `Insert` or `Overstrike`;
- the direction: `Forward` or `Reverse`. It is used by search and by moving over words
  and lines;
- the current line number and the number of lines;
- the word `Learning` while a macro is being recorded.

The **message line** at the bottom shows the last message or a question. Every message
looks the same: `%TPU-<letter>-<NAME>, <text>`. The letter says how serious it is.
See section 15.

The **key hint line** works like in nano: the key is highlighted, the command name is next to
it. As many hints are shown as fit the width. The keys come from the live table, so after
DEFINE KEY the hint changes. The `HINTS` command hides and shows the line; the row goes back
to the text while it is hidden.

The line `[End of file]` shows where the text ends. It is not part of the file.

## 3. The GOLD key

GOLD is a prefix key. It is like Shift, but you do not hold it: press it once, then press
another key. This gives every key a second meaning.

GOLD is **KP/** (the slash on the numeric keypad) or **Esc**. Both do the same.

The keypad follows EDT emulators on a PC: there the VT keys PF1 to PF4 sit on NumLock, /, *
and -. A terminal never sends NumLock, so GOLD moved one key to the right, to /. HELP is F1.

If the keypad slash types `/`, your terminal ignores the keypad mode (see "What to do if...").
Then use Esc: press Esc, release it, press the next key. No need to wait, and no need to hold
Esc.

Example: `KP5` (the 5 on the numeric keypad) sets the direction to "reverse", and
`GOLD` then `KP5` goes to the top of the text (TOP).

GOLD, then digits, then a key repeats that key that many times. `GOLD 5 Down` moves down
five lines. This is how EDT works.

Small and capital letters after GOLD are the same: `GOLD b` and `GOLD B` are both the
BUFFER command.

## 4. Type and edit text

Just type. Letters appear where the cursor is.

| What you want                            | Key                                    |
|------------------------------------------|----------------------------------------|
| New line                                 | Enter                                  |
| Erase the letter left of the cursor      | Backspace (Delete on a VT)             |
| Erase the letter under the cursor        | Delete on a PC (Remove on a VT), KP+   |
| Erase from the cursor to the end of word | Ctrl-J                                 |
| Erase the whole line                     | GOLD KP2                               |
| Erase from the start of line to cursor   | Ctrl-U                                 |
| Bring back the last erased text          | GOLD KP- or GOLD KP+ (RESTORE)         |
| Insert / overstrike                      | Ctrl-A (CHANGE MODE)                   |
| A TAB character                          | Tab                                    |

RESTORE brings back only what was erased by "erase word", "erase line" and "erase start
of line". There is no undo. Be careful.

A TAB is stored as one character and shown as spaces up to the next column that is a
multiple of 8.

Any UTF-8 text works: the cursor moves over whole characters, erase removes a whole
character.

## 5. Move around

| Where                                   | Key                                    |
|-----------------------------------------|----------------------------------------|
| One character left / right              | Left / Right                           |
| One line up / down                      | Up / Down                              |
| Start of line                           | Home, Ctrl-H                           |
| End of line                             | End, Ctrl-E, KP2                       |
| One screen forward                      | Page Down, KP8                         |
| One screen back                         | Page Up, GOLD KP8                      |
| Top of the text                         | GOLD KP5, GOLD Up                      |
| End of the text                         | GOLD KP4, GOLD Down                    |
| Line number N                           | GOLD KP0 or GOLD L, then the number    |
| One word in the current direction       | KP1                                    |
| One word back / forward                 | GOLD Left / GOLD Right                 |
| End (or start) of line                  | KP0 (MOVE BY LINE)                     |
| Direction forward                       | KP4, GOLD + (FORWARD)                  |
| Direction reverse                       | KP5, GOLD - (REVERSE)                  |

The direction is kept for every buffer and shown in the status line.

## 6. Save and leave

| What you want                                    | Key                            |
|--------------------------------------------------|--------------------------------|
| Write everything and leave (EXIT)                | Ctrl-Z, GOLD X                 |
| Leave without writing anything (QUIT)            | GOLD Q                         |
| Write the current buffer, stay (WRITE FILE)      | GOLD W                         |

EXIT writes every changed buffer to its file and, after leaving, prints how many lines went
to which file. If a buffer has no file, the editor asks:
`Write buffer scratch? [Y/N]`. Answer `y` and type a file name, or `n`.

QUIT asks for confirmation if something changed: `Buffer modifications will not be
saved, continue quitting? [Y/N]`.

WRITE FILE without a name writes to the buffer's file. With a name it writes to that
file, and that file becomes the buffer's file.

## 7. Commands: the DO key

Everything in the editor has a command name: `FIND`, `GET FILE`, `TWO WINDOWS` and so
on. You can type any command in words.

1. Press DO: **KP-** (the minus on the numeric keypad), **KP7** or **GOLD Enter**.
2. `Command:` appears at the bottom.
3. Type the command and press Enter.

You can shorten a command as long as it stays clear: `GET F foo.c`, `SH B`, `TWO`.
If the short form fits several commands, the editor says `ambiguous command`.

In every question the editor asks, these keys work:

| Key               | What it does                                  |
|-------------------|-----------------------------------------------|
| Up / Down, Ctrl-B | Recall earlier answers                        |
| Backspace         | Erase a letter                                |
| Ctrl-U            | Erase the whole answer                        |
| Ctrl-Z, Esc       | Cancel the question                           |
| Enter             | Done                                          |

The full command list: DO, then `HELP`.

## 8. Buffers and files

The main thing in the editor is a **buffer**: a piece of text in memory. A buffer may
have a file, or may have none. The buffer `MAIN` is created when you start without a file.

| Command             | Key      | What it does                                                   |
|---------------------|----------|----------------------------------------------------------------|
| `BUFFER name`       | GOLD B   | Show a buffer. If there is none, create an empty one           |
| `BUFFER`            | GOLD B   | Go back to the previous buffer of this window                  |
| `GET FILE name`     | GOLD G   | Read a file into a buffer named after the file and show it     |
| `INCLUDE FILE name` | GOLD I   | Insert a file into the text at the cursor                      |
| `WRITE FILE [name]` | GOLD W   | Write the buffer                                               |
| `SHOW BUFFERS`      | GOLD S, GOLD KP7 | List all buffers                                       |
| `DELETE BUFFER name`|          | Remove a buffer                                                |

System buffers are always there:

- `MESSAGES`: every message of the session. `BUFFER MESSAGES` shows the history;
- `INSERT HERE`: the paste buffer (clipboard);
- `$RESTORE`: the last erased text;
- `HELP`: the help text.

They cannot be written to a file and cannot be deleted. `MESSAGES` and `HELP` cannot be
edited.

## 9. Select, cut, paste

1. Put the cursor at the start of the piece.
2. Press SELECT: the **Select** key, **KP.** (the period on a VT numeric keypad) or **5** on a
   PC keypad with NumLock off.
3. Move the cursor to the end of the piece. The selected text is shown in reverse video.
4. Press REMOVE (**Remove**, **KP6**): the piece is cut into the `INSERT HERE` buffer.
   Or STORE TEXT (**GOLD Remove**, **GOLD KP9**): the piece is copied, the text stays.
5. Put the cursor where you want it and press INSERT HERE (**Insert Here**, **KP9**,
   **GOLD KP6**).

To cancel a selection: SELECT again, or RESET (**KP3**, **GOLD KP.**).

## 10. Find and replace

| Command                 | Key               | What it does                                   |
|-------------------------|-------------------|------------------------------------------------|
| `FIND string`           | Find, GOLD F, GOLD KP* | Search in the current direction           |
| `FIND NEXT`             | KP*               | Search for the same string again               |
| `WILDCARD FIND pattern` |                   | Find a line that matches the pattern as a whole|
| `REPLACE old`           | GOLD R, GOLD KP1, GOLD Find | Replace, asking at every match       |

The case rule is the EVE rule: if the search string has no capital letters, case does
not matter. `world` finds `WORLD` and `World`. If there is at least one capital letter,
the search is exact. This works for any script when the locale is UTF-8.

An empty answer to `Forward find:` repeats the last search.

The search goes in the direction shown in the status line. Not found? Change the
direction (KP4, KP5) or go to the top (GOLD KP5).

WILDCARD FIND matches a whole line: `*` is any string, `%` is exactly one character.
`*foo*` finds a line that contains foo. `%%%:*` finds a line with a colon in column four.

REPLACE asks for the old string, then the new one, then stops at every match:

    Replace? Type Yes, No, All, Last, or Quit:

Press `y` to replace, `n` to skip, `a` to replace all the rest without asking, `l` to
replace this one and stop, `q` to stop. At the end you see `N replacements made`.

## 11. Two windows

| Command         | Key           | What it does                                  |
|-----------------|---------------|-----------------------------------------------|
| `TWO WINDOWS`   | GOLD T        | Split the screen into two windows             |
| `OTHER WINDOW`  | GOLD O        | Move to the other window                      |
| `ONE WINDOW`    | GOLD Z        | Back to one window                            |

At first both windows show the same buffer. In any window you can do `BUFFER` or
`GET FILE` and look at two files at once. The window with the cursor has a brighter
status line.

The windows follow the size of the terminal. If a terminal on a serial line cannot
report a resize, press Ctrl-W (REFRESH).

## 12. Macros: LEARN, REPEAT, DEFINE KEY

**Record a macro.**

1. Press Ctrl-K (LEARN). `Learning` appears in the status line.
2. Do what you want to repeat. Keys are recorded, including answers to questions.
3. Press Ctrl-R (REMEMBER). The editor asks which key should hold the macro. Press it,
   for example F6.

Now F6 repeats what you recorded. If you press Enter or Ctrl-Z instead of a key,
the recording is thrown away.

**Repeat many times.** `GOLD 9 9 9 F6`, or DO, `REPEAT 999`, then F6. The repeat
stops as soon as a command inside fails. So `REPEAT 999` with a macro that contains
FIND walks through the whole text and stops at the end by itself.

**Put a command on a key.** GOLD D (DEFINE KEY), type the command, for example `TOP`,
press the key. GOLD K (SHOW KEYS) shows every binding.

Bindings live until you leave the editor, unless you save them.

**Save the bindings to a file.** DO, `SAVE KEYS`, Enter: `tpu.keys` appears in the current
directory with every binding, macros included. At the next start from this directory the file is
read by itself. The order at start: `~/.tpu.keys`, then `./tpu.keys`, then the file from the
`-keys` option or the `TPU_KEYS` environment variable. Each file overrides the previous one. To read
a file at any time: `LOAD KEYS file`.

The file is simple, one line per binding, `!` starts a comment:

    KP/, Esc                   = GOLD
    GOLD-Left                  = PREVIOUS WORD
    F6                         = KEYS "ab{Return}{GOLD-KP1}"   ! a recorded macro
    GOLD-k                     = NONE                        ! remove the binding

Key names are those SHOW KEYS shows: `F1`, `KP7`, `Insert Here`, `Begin`, `Ctrl-K`, `Delete`,
`Space`, `Return`, `Tab`, `Escape`, a letter, `#hex` for anything else. Several keys go through a
comma, a blank on both sides of `=`. `KEYS "..."` is a macro: text as itself, special keys in
braces, a quote, a backslash and `{` written with a backslash before them. A bad line is reported
with its number and skipped, the rest is read.

## 13. Journal and recovery after a crash

Every key you press is written to a journal file next to your file: `notes.txt.tjl`
(or `tpu.tjl` when started without a file). On a normal exit the journal is deleted.

If the editor crashed, the terminal went away, or the process was killed:

    ./build/tpu -recover notes.txt

The editor reads the file as it was and replays every key from the journal. You see
`%TPU-I-RECOVERING, N keys from notes.txt.tjl`. Then save right away: Ctrl-Z.

Important: the file on disk must be the same as at the start of that session. If it
changed since, the replay may stop with `RECOVSTOP`.

## 14. Help inside the editor

| Type after DO        | What you get                               |
|----------------------|--------------------------------------------|
| `HELP`               | General help and the list of topics        |
| `HELP LEARN`         | Help on one command                        |
| `HELP KEYS`          | Every key binding, as it is right now      |
| `SHOW KEYS`          | The same; keys GOLD K, GOLD F1             |
| `HINTS`              | Hide or show the key hint line             |

Keys: F1, GOLD H, GOLD ?. The key table: GOLD K or GOLD F1.

Help works like HELP on VMS. `HELP` without a topic shows the introduction and the list of topics
in columns, with the `Topic?` prompt below. Type a topic, an abbreviation will do, and press Enter.
An empty Enter on a topic goes back to the list, an empty Enter on the list leaves, Ctrl-Z leaves
at once. The arrows, Page Up and Page Down scroll the text. The key table is laid out in columns
across the screen width too.

## 15. Messages: what they mean and what to do

A message looks like this:

    %TPU-E-NOTFOUND, could not find: foo

- `TPU` is who speaks;
- the letter is the severity: `S` success, `I` information, `W` warning, `E` error,
  `F` fatal;
- `NOTFOUND` is the message name. Look it up in the tables below;
- then the text.

Errors `E` and `F` stop a macro replay and a journal recovery. All messages are kept
in the `MESSAGES` buffer.

### Files

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| NEWFILE    | I, `file: new file`                  | The file does not exist. This is fine: it is created when you write. |
| OPENIN     | E, `cannot open file: reason`        | The file cannot be read. Check the name, the path and the permissions. The reason after the colon comes from the system. |
| READ       | S, `N lines read from file`          | The file is loaded. |
| OPENOUT    | E, `cannot create file: reason`      | The file cannot be created. Missing directory, no permission, or no space. Try WRITE FILE with another name. |
| WRITEERR   | E, `error writing file: reason`      | The write broke off, the file may be incomplete. Free some space and write again, better under another name. |
| WRITTEN    | S, `N lines written to file`         | Written. |

### Buffers

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| READONLY   | E, `buffer X is read-only`           | `MESSAGES`, `HELP` and the buffer and key listings cannot be edited. Go to your own buffer: GOLD B. |
| NOPREVBUF  | E, `no previous buffer`              | `BUFFER` without a name, but this window had no other buffer yet. Give a name. |
| BUFCREATED | I, `buffer X created`                | An empty buffer without a file was created. |
| BUFEXISTS  | I, `buffer X already exists`         | GET FILE of a file that is already open. The old buffer is shown, the file was not read again. |
| SYSBUF     | E, `buffer X is a system buffer`     | A system buffer cannot be written to a file or deleted. |
| NOSUCHBUF  | E, `no such buffer: X`               | A typo in the name. `SHOW BUFFERS` lists the names. |
| NOTDELETED | E, `buffer X not deleted`            | You answered `n` to the delete question. |
| BUFDELETED | S, `buffer X deleted`                | Deleted. |

### Selection, paste, movement

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| NOSELECT   | E, `no select range active`          | Press Select (KP.) first, then move the cursor. |
| EMPTYSEL   | E, `select range is empty`           | Select was pressed but the cursor did not move. Move it. |
| SELECT     | I, `move the cursor to select text`  | The selection started. |
| SELCANCEL  | I, `selection cancelled`             | The selection is off. |
| STORED     | I, `text copied to the INSERT HERE buffer` | Copied, the text stays. |
| NOTHING    | E, `nothing to insert` / `nothing to restore` | The paste buffer is empty, or nothing was erased yet. |
| ENDOFBUF   | E, `already at end of buffer`        | Nothing further to scroll to. |
| TOPOFBUF   | E, `already at start of buffer`      | Nothing further back. |
| BADLINE    | E, `bad line number: X`              | A line number is a whole number from 1. |

### Find and replace

| Name        | Text                                | What it means and what to do |
|-------------|-------------------------------------|------------------------------|
| NOSEARCH    | E, `no search string`               | An empty search and no earlier search. Type what to look for. |
| NOTFOUND    | E, `could not find: X`              | Not found in the current direction. Change the direction (KP4, KP5) or go to the top (GOLD KP5). Check the case: a capital letter in the string asks for an exact match. |
| NOMATCHLINE | E, `could not find a line matching: X` | The pattern must match the whole line. Add `*` at both ends: `*foo*`. |
| REPLACED    | I, `N replacements made`            | Replace finished. |

### Commands and keys

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| CANCEL     | E, `cancelled`                       | You cancelled a question (Ctrl-Z, Esc). Nothing is broken. |
| UNKCMD     | E, `unknown command: X`              | No such command. DO, `HELP` shows the list. |
| AMBCMD     | E, `ambiguous command: X`            | The short form fits several commands. Type more letters. |
| KEYUNDEF   | E, `key X is not defined`            | Nothing is on that key. Define it: GOLD D. List: GOLD K. |
| KEYDEFINED | I, `key X defined as Y`              | Defined. |
| KEYTBLFULL | E, `key definition table is full`    | More than 512 definitions. Redefine old ones. |
| BADCOUNT   | E, `bad repeat count: X`             | The repeat count is 1 to 32768. |
| PRESSRET   | I, `press Return to continue`        | You are looking at help or a listing. Movement keys scroll, any other key brings you back. |
| NOHELP     | E, `no help on X`                    | No such topic. `HELP` without a topic shows the list. |
| QUITCANCEL | E, `quit cancelled`                  | You answered `n`. The editor goes on. |
| EXITCANCEL | E, `exit cancelled`                  | No file name was given for a buffer without a file. Exit is off, nothing is lost. |
| HINTSON    | I, `key hint line on`                | The hint line is shown. |
| HINTSOFF   | I, `key hint line off`               | The hint line is hidden, `HINTS` brings it back. |

### Key files

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| KEYSLOADED | I, `N key definitions loaded from X` | The file is read. |
| KEYSERRORS | E, `N key definitions loaded from X, M bad lines` | Some lines were bad and skipped. The details, line by line, are in the `MESSAGES` buffer. |
| KEYSSAVED  | S, `N key definitions written to X`  | The bindings are written. |
| BADKEYLINE | E, `X line N: expected keys = command` | The line has no `=` with a blank before it. The format is `keys = command`. |
| BADKEY     | E, `X line N: unknown key "..."`     | No key of that name. See SHOW KEYS for the names. |
| BADKEYCMD  | E, `X line N: unknown command "..."` | No such command. Use the full name as in the HELP list. |
| BADMACRO   | E, `X line N: bad KEYS string`       | The `KEYS "..."` has no closing quote or brace, or an unknown name in `{}`. |

### Macros

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| LEARNING   | I, `learning, press Ctrl-R to remember` | Recording. |
| ALRLEARN   | E, `already learning, ...`           | Ctrl-K pressed twice. Finish the recording: Ctrl-R. |
| NOTLEARN   | E, `not learning`                    | Ctrl-R without Ctrl-K. Start a recording first. |
| NOLEARN    | E, `nothing learned`                 | Ctrl-R right after Ctrl-K. Press some keys in between. |
| DISCARDED  | E, `learned sequence discarded`      | Enter or Ctrl-Z was pressed instead of a key for the macro. The recording is gone. |
| LEARNOVF   | E, `learn sequence too long, ...`    | More than 4096 keys. Split the macro. |
| KEYLEARNED | I, `key X defined as a learned sequence of N keys` | The macro is on the key. |
| QUEOVF     | E, `key playback queue overflow, sequence aborted` | The repeat is too big, or a macro calls itself. Use a smaller count. |

### Windows

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| ONEWINDOW  | I, `already one window`              | There is one window already. |
| TWOWINDOWS | I, `already two windows`             | There are two windows already. |
| ONLYONEWIN | E, `only one window`                 | OTHER WINDOW with one window. GOLD T first. |

### Journal

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| JNLCREATE  | E, `cannot create journal X: reason` | The directory is not writable. The editor goes on without a journal. Start from another directory or fix the permissions. |
| JNLOPEN    | E, `cannot open journal X: reason`   | Started with `-recover`, but there is no journal. The editor leaves. Start without `-recover`. |
| RECOVERING | I, `recovering N keys from X`        | The replay is running. |
| RECOVSTOP  | E, `recovery stopped at key N of M: reason` | A replayed key failed, most often because the file changed since that session. The rest of the journal is dropped. Look at what came back and save it. |

### Internal

| Name       | Text                                 | What it means and what to do |
|------------|--------------------------------------|------------------------------|
| NOMSG      | F, `message number X is not in the catalogue` | A bug in the editor itself. Save your text and report the number. |

### Messages outside the catalogue

These lines go to the standard error stream, not to the editor screen. They start with
the date, the time and `%TPU-E:` or `%TPU-I:`.

| Text                                        | What it means and what to do |
|---------------------------------------------|------------------------------|
| `usage: tpu [-recover] [-nojournal] [file]` | A wrong start option. |
| `standard input is not a terminal`          | The editor was started outside a terminal (in a pipe or from a script). Start it in a terminal. |
| `tcgetattr(): errno=N, ...`, `tcsetattr(): ...` | The terminal cannot be switched to raw mode. The reason follows `errno`. |
| `input closed, journal X is kept`           | The terminal closed while editing. The editor left with exit code 2 and kept the journal: start with `-recover`. |
| `default key table refers to unknown command` | A build error in the editor. Report it. |

## 16. What to do if...

**A key does not work and you do not know why.** Run `./build/keycode`, press the key, then
Ctrl-C. The utility prints the bytes the terminal sent and the name TPU gives the key. The line
`unknown to TPU` means the terminal sends a sequence that is not in the table `tpukeys.h`: add
it there. The `-normal` option leaves the numeric keypad in its normal mode, to compare.

    ./build/keycode
    bytes                    as text              TPU key
    1b 4f 53                 ESC O S              F4
    1b 5b 36 7e              ESC [ 6 ~            Next Screen
    03                       ^C                   Ctrl-C

**The keypad / and * type text, GOLD on KP/ does not work.** The terminal ignores the
keypad mode. Use Esc as GOLD. Check with `keycode`: in the right mode the keypad slash sends
`ESC O o`, the star `ESC O j`, the minus `ESC O m`, the plus `ESC O k`.

**NumLock opens HELP.** Some emulators (Tera Term and the like) turn NumLock into the VT key
PF1 by themselves. PF1 sends the same code as F1, and F1 is HELP. Use KP/ or Esc for GOLD.

**The numeric keypad types digits instead of commands.** The terminal must send the
"application mode" keypad codes. The editor turns that mode on, but most PC emulators do not
obey: with NumLock on the keypad sends digits, with NumLock off it sends the cursor block codes
(Home, arrows, PgUp and so on). That keypad has a map of its own:

    7 Home  START OF LINE   GOLD: TOP           /  text    GOLD: HELP
    8 Up    MOVE UP         GOLD: TOP           *  text    GOLD: SHOW KEYS
    9 PgUp  PREVIOUS SCREEN                     -  text    GOLD: REVERSE
    4 Left  MOVE LEFT       GOLD: PREVIOUS WORD +  text    GOLD: FORWARD
    5 Begin SELECT          GOLD: RESET
    6 Right MOVE RIGHT      GOLD: NEXT WORD
    1 End   END OF LINE     GOLD: BOTTOM
    2 Down  MOVE DOWN       GOLD: BOTTOM
    3 PgDn  NEXT SCREEN
    0 Ins   INSERT HERE     GOLD: STORE TEXT
    . Del   REMOVE          GOLD: STORE TEXT
    Enter   RETURN          GOLD: DO

GOLD here is Esc. Select with 5, cut with the period, paste with 0. The `keycode` utility
shows what your emulator sends.

**Home and End do not move the cursor.** Some terminals (the Linux console) send the
Find and Select codes for them. Use Ctrl-H and Ctrl-E, and GOLD F for find.

**The screen is garbage.** Press Ctrl-W (REFRESH): the editor asks for the size again
and redraws everything.

**`Screen too small` is shown.** The terminal has fewer than 4 rows or 20 columns. Make
the window bigger.

**I pressed GOLD and nothing happens.** The editor waits for the second key. Press it,
or press something harmless like an arrow.

**F1 does not open HELP.** F1 works when the terminal sends `ESC O P` or `ESC [ 11 ~` for it.
Some terminals need a setting for that; otherwise use GOLD H. The other F keys have no
command by default: put one there with GOLD D (DEFINE KEY) or a key file.

**Search in another language does not ignore case.** Start with `LANG=C.UTF-8`.
Without a UTF-8 locale, only Latin letters are compared without case.

**I cannot undo the last change.** There is no undo. RESTORE (GOLD KP-, GOLD KP+) brings back only
the last erased word, line or start of line. If the file is not written yet, you can
leave with QUIT and open it again.

**A `.tjl` file is left next to my file.** The last session did not end with EXIT or
QUIT. To get the work back, start with `-recover`. If you do not need it, delete the file.

**`-recover` stopped half way.** The file changed after that session, or the session
ran in a terminal of another size. What came back is already in the buffer. Save it and
finish by hand.

**The terminal acts strange after leaving.** This happens after a crash. Type `reset`
and Enter.

**Lines `%TPU-...` appear on stderr.** That is `$LOG` of the StarLet library: it writes
there before the editor takes the screen and after it gives it back. While the editor is
on the screen the log goes to `/dev/null`. A debug build (`-DCMAKE_BUILD_TYPE=Debug`)
writes everything to `tpu.log` in the current directory.

## 17. Key table

Short form. The full layout on PC keys is in [KEYS.txt](KEYS.txt); the live table is
`HELP KEYS`.

    F1   HELP                  KP/ / Esc  GOLD         KP*  FIND NEXT          KP-  DO
    KP0  MOVE BY LINE          KP1  MOVE BY WORD       KP2  END OF LINE        KP3  RESET
    KP4  FORWARD               KP5  REVERSE            KP6  REMOVE             KP7  DO
    KP8  NEXT SCREEN           KP9  INSERT HERE        KP+  ERASE CHARACTER    KP,  ERASE CHARACTER
    KP.  SELECT                Enter RETURN
    GOLD-KP0 LINE     GOLD-KP1 REPLACE   GOLD-KP2 ERASE LINE   GOLD-KP4 BOTTOM   GOLD-KP5 TOP
    GOLD-KP6 INSERT HERE   GOLD-KP7 SHOW BUFFERS   GOLD-KP8 PREVIOUS SCREEN   GOLD-KP9 STORE TEXT
    GOLD-KP- RESTORE   GOLD-KP+ RESTORE   GOLD-KP, RESTORE   GOLD-KP. RESET   GOLD-KP* FIND   GOLD-F1 SHOW KEYS
    Find FIND   Insert Here INSERT HERE   Remove REMOVE   Select SELECT   Prev/Next Screen
    Home START OF LINE   End END OF LINE   Up/Down/Left/Right   GOLD-Up TOP   GOLD-Down BOTTOM
    PC keypad: 5 SELECT   GOLD-5 RESET   GOLD-Enter DO   GOLD-Home TOP   GOLD-End BOTTOM
    GOLD-Left PREVIOUS WORD   GOLD-Right NEXT WORD   GOLD-Insert STORE TEXT
    GOLD-/ HELP   GOLD-* SHOW KEYS   GOLD-- REVERSE   GOLD-+ FORWARD
    Ctrl-Z EXIT    GOLD-X EXIT    GOLD-Q QUIT    F2-F20: no command, free for DEFINE KEY
    Ctrl-A CHANGE MODE   Ctrl-E END OF LINE   Ctrl-H START OF LINE   Ctrl-J ERASE WORD
    Ctrl-K LEARN   Ctrl-R REMEMBER   Ctrl-U ERASE START OF LINE   Ctrl-W REFRESH
    GOLD-letter: B BUFFER, D DEFINE KEY, F FIND, G GET FILE, H HELP, I INCLUDE FILE, K SHOW KEYS, L LINE,
                 O OTHER WINDOW, Q QUIT, R REPLACE, S SHOW BUFFERS, T TWO WINDOWS, W WRITE FILE,
                 X EXIT, Z ONE WINDOW, ? HELP
