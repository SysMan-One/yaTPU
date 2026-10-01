#define	__MODULE__	"TPU"
#define	__IDENT__	"X-00-14"
#define	__REV__		"00.14.00"

/*
**++
**
**  FACILITY:  TPU - a lightweight text editor in the spirit of DEC TPU/EVE
**
**  ABSTRACT:  Buffers are first-class objects, a file is an optional attribute of a buffer.
**	Windows are viewports onto buffers. All input goes through s_key_get(), so LEARN/REPEAT,
**	the journal and RECOVER are one and the same mechanism: a queue of keys.
**
**  DESIGN ISSUES:
**	- text storage: array of TPU$LINE pointers per buffer, one malloc'ed string per line;
**	- screen: VT100 escapes, no ncurses; shadow screen, only changed spans are rewritten;
**	- size of the screen is taken from TIOCGWINSZ, falls back to a DSR/CPR query on a serial line;
**	- text is bytes; UTF-8 is understood only where it matters: the cursor moves over whole
**	  characters and display columns are counted with wcwidth(). TAB is expanded on display only;
**	- no undo, RESTORE only;
**	- messages are VMS condition codes from the catalogue in tpumsg.h, registered with StarLet;
**	  $LOG/$TRACE go to the standard error, and away from the screen while the terminal is in raw mode.
**
**  AUTHORS:   StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  10-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	10-SEP-2026	RRL	X-00-01 (REV 00.01.00) : Initial revision.
**	28-SEP-2026	RRL	X-00-02 (REV 00.02.00) : Brought to the StarLet coding rules: s_ prefix on static routines,
**				_t struct tags with TPU$ typedefs, routine headers in the VMS layout, $LOG instead of
**				fprintf(stderr) with errno reported, block-scoped loop counters.
**	28-SEP-2026	RRL	X-00-02ECO1 (REV 00.02.01) : NEXT SCREEN scrolled past the end of a short buffer, leaving
**				only [End of file] on the screen; the window top is now capped so the window stays full.
**	28-SEP-2026	RRL	X-00-02ECO2 (REV 00.02.02) : EXIT left silently; the messages of the files written on
**				the way out are now printed after the terminal is restored, as EVE does.
**	28-SEP-2026	RRL	X-00-03 (REV 00.03.00) : SHOW KEYS command, the live key table on GOLD-K and GOLD-PF2 (GOLD-F2);
**				HELP KEYS is the same listing. WRITTEN message on EXIT had an empty file name: s_buf_write()
**				copied the buffer's file name onto itself.
**	28-SEP-2026	RRL	X-00-04 (REV 00.04.00) : GOLD also on KP- (keypad minus), PF1 stays; ERASE WORD keeps Ctrl-J and F13,
**				GOLD-KP- is still RESTORE.
**	28-SEP-2026	RRL	X-00-05 (REV 00.05.00) : Key hint line above the message line, nano style: the key in reverse
**				video and the command next to it, taken from the live key table; HINTS toggles it.
**	28-SEP-2026	RRL	X-00-06 (REV 00.06.00) : Key codes and escape sequences moved to tpukeys.h, shared with the
**				new keycode utility that shows what the terminal sends for a key.
**	29-SEP-2026	RRL	X-00-07 (REV 00.07.00) : PC keypad map for an emulator without application keypad mode: Esc is
**				GOLD, keypad 5 (Begin) is SELECT, GOLD-Enter DO, GOLD-Home/End TOP/BOTTOM, GOLD-Left/Right the
**				new PREVIOUS WORD / NEXT WORD, GOLD-minus/plus REVERSE/FORWARD, GOLD-/ HELP, GOLD-* SHOW KEYS,
**				GOLD-Insert STORE TEXT, GOLD-Begin RESET.
**	29-SEP-2026	RRL	X-00-08 (REV 00.08.00) : The selection is shown in reverse video, from the SELECT anchor to the
**				cursor, a newline inside it as one extra cell; the hint row's in-row marks do the job.
**	29-SEP-2026	RRL	X-00-09 (REV 00.09.00) : Key files: "keys = command" lines with KEYS "..." macros and NONE,
**				loaded from ~/.tpu.keys, ./tpu.keys, -keys file or TPU_KEYS at start and by LOAD KEYS, written
**				by SAVE KEYS; SHOW KEYS prints a learned sequence in the same notation.
**	29-SEP-2026	RRL	X-00-10 (REV 00.10.00) : HELP in the VMS way: the introduction with the topics in columns,
**				one topic at a time, a "Topic? " prompt that scrolls with the navigation keys; SHOW KEYS in
**				columns as well.
**	01-OCT-2026	RRL	X-00-10ECO1 (REV 00.10.01) : "увеличь таймаут ESC" - the wait for the next byte of an
**				escape sequence is 250 ms instead of 80 (TPU$K_ESCWAIT), a key split by a tunnel (yaVTERM)
**				or a slow line is no longer read as GOLD and stray letters. ESC followed by a byte that
**				starts no sequence, or by one byte and then a pause, is GOLD and that key, not lost.
**	01-OCT-2026	RRL	X-00-11 (REV 00.11.00) : "EDT-сдвиг, F-клавиши убрать все кроме F1" - the keypad for a PC:
**				F1 is HELP (GOLD-F1 SHOW KEYS), the PF row moves to / * - the way EDT emulators put it on
**				NumLock / * -, NumLock sending nothing: KP/ GOLD, KP* FIND NEXT (GOLD- FIND), KP- DO
**				(GOLD- RESTORE), KP+ ERASE CHARACTER (GOLD- RESTORE) like the VT's KP,. F2-F20, Help and
**				Do are no longer bound; Esc stays GOLD. The hint row shows F1 HELP and KP- DO.
**	01-OCT-2026	RRL	X-00-12 (REV 00.12.00) : "делай ближе к EDT: минус ERASE LINE, 3 MOVE BY CHAR" - KP- is
**				ERASE LINE (GOLD-KP- RESTORE), as EDT's DEL L / UND L; new MOVE BY CHAR (a character in the
**				current direction, EDT's CHAR) on KP3. DO stays on KP7 and GOLD-Enter.
**	01-OCT-2026	RRL	X-00-13 (REV 00.13.00) : "insert here пусть будет на insert, а DO на минус": KP- is DO again
**				(KP7 and GOLD-Enter too, GOLD-KP- RESTORE), ERASE LINE moves to KP9 (GOLD-KP9 STORE TEXT),
**				INSERT HERE stays on Insert and GOLD-KP6 only.
**	01-OCT-2026	RRL	X-00-14 (REV 00.14.00) : Closer to EDT, as compared with EDT on the guest: KP0 is the new
**				MOVE BY LINE START (EDT's LINE: start of the next line, of this or the previous one in
**				reverse), GOLD-KP0 OPEN LINE, GOLD-KP1 CHANGE CASE (the selection, or the character under
**				the cursor and a step on), GOLD-KP2 ERASE END OF LINE (EDT's DEL EOL, RESTORE brings it
**				back). LINE stays on GOLD-L, REPLACE on GOLD-R, MOVE BY LINE has no default key.
**
**--
*/

#define	_GNU_SOURCE	1				/* wcwidth(), getline()			*/

#define	__FAC__		"TPU"
#define	__TFAC__	__FAC__ ": "
#include	"utility_routines.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>
#include	<unistd.h>
#include	<errno.h>
#include	<ctype.h>
#include	<signal.h>
#include	<stdarg.h>
#include	<termios.h>
#include	<poll.h>
#include	<fcntl.h>
#include	<sys/ioctl.h>
#include	<sys/stat.h>
#include	<locale.h>
#include	<wchar.h>
#include	<wctype.h>

#include	"tpu_help.h"					/* Generated from tpu.hlp at build time		*/
#include	"tpumsg.h"					/* Message catalogue				*/
#include	"tpukeys.h"					/* Key codes and escape sequences		*/

/* Condition code status: only ERROR/FATAL are failures, INFO and WARN are fine - the same rule
 * as StarLet's $STS_ERROR(), but a function, not a macro: $STS_ERROR() names its argument twice
 * and the argument here is usually a call with side effects. */
static inline int	TPU$FAILED(int a_sts)
{
	return	((a_sts & 7) == STS$K_ERROR) || ((a_sts & 7) == STS$K_FATAL);
}

/*
 * Text storage
 */
typedef	struct	line_t {
	unsigned	len, alloc;
	char *		text;					/* Not NUL-terminated, len is the truth	*/
} TPU$LINE;

#define	BUF$M_MODIFIED	(1 << 0)
#define	BUF$M_NOWRITE	(1 << 1)				/* Read-only for the user		*/
#define	BUF$M_SYSTEM	(1 << 2)				/* Never written, never asked about	*/

typedef	struct	buffer_t {
	struct buffer_t *next;

	char		name[64];
	char		file[256];				/* Empty: buffer is not tied to a file	*/

	TPU$LINE **		lines;
	unsigned	nlines, alloc;				/* Invariant: nlines >= 1		*/

	unsigned	cur_line, cur_col;			/* cur_col may be == len (past end)	*/
	int		mark_line, mark_col;			/* SELECT anchor, -1 - no selection	*/

	unsigned	flags;
	int		overstrike, reverse;			/* Per-buffer modes, as in EVE		*/
} TPU$BUFFER;

typedef	struct	pos_t {
	unsigned	line, col;
} TPU$POS;

/*
 * Window: a viewport onto a buffer, status line at row0 + nrows
 */
typedef	struct	window_t {
	TPU$BUFFER *	buf, *prev;
	int		row0, nrows;				/* Text rows, status line is not counted	*/
	unsigned	top, left;				/* First visible line, first visible column	*/
} TPU$WINDOW;

/*
 * Commands and key bindings
 */
#define	CMD$M_NAV	1					/* Allowed while browsing HELP / SHOW BUFFERS	*/

typedef	struct	cmd_t {
	const char *	name;
	int		(*fn)(char *a_arg);
	unsigned	flags;
	const char *	help;					/* One-liner for the HELP index		*/
} TPU$CMD;

#define	KD$K_CMD	1
#define	KD$K_MACRO	2

typedef	struct	keydef_t {
	int		key;
	int		type;
	const TPU$CMD *	cmd;
	int *		seq;					/* Learned sequence, KD$K_MACRO		*/
	int		seqlen;
} TPU$KEYDEF;

/*
 * Globals
 */
#define	TPU$K_PBSZ	65536					/* Keys in the playback queue		*/
#define	TPU$K_LEARNSZ	4096					/* Keys in one learned sequence		*/
#define	TPU$K_MAXKEYDEF	512
#define	TPU$K_TABSZ	8

static TPU$BUFFER *	s_buffers;					/* Registry of buffers			*/
static TPU$BUFFER *	s_msgbuf, *s_pastebuf, *s_restorebuf, *s_helpbuf;

static TPU$WINDOW	s_win[2];
static int	s_nwin = 1, s_curwin;
#define	CURWIN	(&s_win[s_curwin])
#define	CURBUF	(s_win[s_curwin].buf)

static int	s_rows = 24, s_cols = 80;
static int	s_cpr_rows, s_cpr_cols;
static volatile sig_atomic_t s_winch;
static int	s_hangup;
static int	s_term_pushback = -1;				/* A byte read after ESC, given back	*/

#define	TPU$K_ESCWAIT	250					/* ms for the next byte of an escape seq	*/

typedef	struct	srow_t {
	char *		text;					/* One screen row, NUL-terminated	*/
	int		rev;					/* Video attribute, -1 - unknown	*/
} TPU$SROW;
static TPU$SROW *	s_shadow, *s_want;
static char	s_out[65536];
static int	s_outlen;

static char	s_msg[512];

#define	TPU$K_MARKATTR	3					/* s_scr_put() attribute: a row with marks inside	*/
#define	TPU$K_MARK	'\001'					/* Toggles reverse video inside such a row: the	*/
								/* key hints, the selection			*/
static int	s_hints = 1;					/* Key hint line shown, HINTS toggles it	*/
static const char *s_prompt_label;				/* Non-NULL while a prompt is active	*/
static const char *s_prompt_text;

static int	s_pb[TPU$K_PBSZ];
static int	s_pb_head, s_pb_tail;

static int	s_learn[TPU$K_LEARNSZ];
static int	s_learn_len, s_learning;

static FILE *	s_journal;
static char	s_journal_name[256];

static TPU$KEYDEF	s_keydefs[TPU$K_MAXKEYDEF];
static int	s_nkeydefs;

static ASC	s_findstr;					/* Last search string			*/

static struct termios s_tio_saved;
static int	s_term_raw;

/* Forward declarations */
static int	s_tpu_msg(unsigned a_code, ...);
static int	s_key_get(void);
static int	s_dispatch(int a_key);
static void	s_scr_render(void);
static int	s_buf_append_line(TPU$BUFFER *a_buf, const char *a_text, unsigned a_len);
static int	s_pb_push_front(const int *a_seq, int a_n);
static void	s_pb_flush(void);
static void	s_log_redirect(int a_raw);
static int	s_sel_bounds(TPU$BUFFER *a_b, TPU$POS *a_from, TPU$POS *a_to);
static TPU$KEYDEF *	s_keydef_find(int a_key);
static const char *	s_key_name(int a_key, char *a_buf, size_t a_sz);

static int	s_logfd = -1;					/* Where $LOG/$TRACE go, see s_log_redirect()	*/

/*
 * =====================================================================================
 *	UTF-8: bytes stay bytes, we only need to know where a character starts and how
 *	wide it is on the screen.
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Is the byte a UTF-8 continuation byte (10xxxxxx)?
 *
 *  INPUTS:
 *	a_c		the byte
 *
 *  RETURNS: non-zero if it is
 */
static int	s_utf8_cont (
	unsigned char	a_c
	)
{
	return	(a_c & 0xC0) == 0x80;
}

/*
 *  DESCRIPTION: Length in bytes of the UTF-8 character starting at a_p; malformed or truncated input counts
 *  as one byte
 *
 *  INPUTS:
 *	a_p		start of the character
 *	a_max		bytes available from a_p
 *
 *  RETURNS: 1..4
 */
static int	s_utf8_len (
	const char	*a_p,
	int		a_max
	)
{
unsigned char	l_c = *a_p;
int	l_n = (l_c < 0x80) ? 1 : (l_c >> 5) == 6 ? 2 : (l_c >> 4) == 14 ? 3 : (l_c >> 3) == 30 ? 4 : 1;

	if ( l_n > a_max )
		return	1;

	for ( int l_i = 1; l_i < l_n; l_i++ )
		if ( !s_utf8_cont(a_p[l_i]) )
			return	1;

	return	l_n;
}

/*
 *  DESCRIPTION: Code point of the character at a_p
 *
 *  INPUTS:
 *	a_p		start of the character
 *	a_n		its length in bytes, as told by s_utf8_len()
 *
 *  RETURNS: the code point; a single byte is returned as is
 */
static wchar_t	s_utf8_decode (
	const char	*a_p,
	int		a_n
	)
{
wchar_t	l_wc;

	if ( a_n == 1 )
		return	(unsigned char) a_p[0];

	l_wc = (a_n == 2) ? (a_p[0] & 0x1F) : (a_n == 3) ? (a_p[0] & 0x0F) : (a_p[0] & 0x07);
	for ( int l_i = 1; l_i < a_n; l_i++ )
		l_wc = (l_wc << 6) | (a_p[l_i] & 0x3F);

	return	l_wc;
}

/*
 *  DESCRIPTION: Encode a code point as UTF-8
 *
 *  INPUTS:
 *	a_wc		the code point
 *
 *  OUTPUTS:
 *	a_out		receives up to 4 bytes, not NUL-terminated
 *
 *  RETURNS: number of bytes written, 1..4
 */
static int	s_utf8_encode (
	wchar_t	a_wc,
	char	*a_out
	)
{
	if ( a_wc < 0x80 )
		return	a_out[0] = a_wc, 1;
	if ( a_wc < 0x800 )
		return	a_out[0] = 0xC0 | (a_wc >> 6), a_out[1] = 0x80 | (a_wc & 0x3F), 2;
	if ( a_wc < 0x10000 )
		return	a_out[0] = 0xE0 | (a_wc >> 12), a_out[1] = 0x80 | ((a_wc >> 6) & 0x3F), a_out[2] = 0x80 | (a_wc & 0x3F), 3;

	a_out[0] = 0xF0 | (a_wc >> 18), a_out[1] = 0x80 | ((a_wc >> 12) & 0x3F);
	a_out[2] = 0x80 | ((a_wc >> 6) & 0x3F), a_out[3] = 0x80 | (a_wc & 0x3F);
	return	4;
}

/*
 *  DESCRIPTION: Screen width of the character at a_p: 1 for ASCII and malformed input, wcwidth() otherwise
 *
 *  INPUTS:
 *	a_p		start of the character
 *	a_n		its length in bytes
 *
 *  RETURNS: number of display columns, at least 1
 */
static int	s_utf8_width (
	const char	*a_p,
	int		a_n
	)
{
int	l_w;

	if ( a_n == 1 )
		return	1;

	return	(l_w = wcwidth(s_utf8_decode(a_p, a_n))) < 0 ? 1 : l_w;
}

/*
 *  DESCRIPTION: Lower-case a byte string character by character; towlower() follows the locale, so any script
 *  it knows is handled
 *
 *  INPUTS:
 *	a_src		source bytes
 *	a_n		their length
 *	a_dstsz		size of a_dst
 *
 *  OUTPUTS:
 *	a_dst		lower-cased, NUL-terminated copy, truncated if a_dstsz is too small
 *
 *  RETURNS: length of the result in bytes
 */
static int	s_utf8_lower (
	const char	*a_src,
	int		a_n,
	char		*a_dst,
	int		a_dstsz
	)
{
int	l_c, l_o = 0;

	for ( int l_i = 0; l_i < a_n && l_o + 4 < a_dstsz; l_i += l_c )
		{
		l_c = s_utf8_len(a_src + l_i, a_n - l_i);
		l_o += s_utf8_encode(towlower(s_utf8_decode(a_src + l_i, l_c)), a_dst + l_o);
		}

	a_dst[l_o] = '\0';
	return	l_o;
}

/*
 *  DESCRIPTION: Does the byte string end in the middle of a multibyte character? Used to hold the redraw
 *  while a character is being typed byte by byte
 *
 *  INPUTS:
 *	a_s		the bytes
 *	a_len		their length
 *
 *  RETURNS: non-zero if the last character is incomplete
 */
static int	s_utf8_incomplete (
	const char	*a_s,
	int		a_len
	)
{
int	l_n;

	for ( int l_i = a_len - 1; l_i >= 0 && l_i >= a_len - 4; l_i-- )
		if ( !s_utf8_cont(a_s[l_i]) )
			{
			unsigned char l_c = a_s[l_i];
			l_n = (l_c < 0x80) ? 1 : (l_c >> 5) == 6 ? 2 : (l_c >> 4) == 14 ? 3 : (l_c >> 3) == 30 ? 4 : 1;
			return	l_n > a_len - l_i;
			}

	return	0;
}

/*
 *  DESCRIPTION: Move a byte offset back to the start of the character it falls into
 *
 *  INPUTS:
 *	a_l		the line
 *	a_col		a byte offset, may be past the end of the line
 *
 *  RETURNS: the aligned offset
 */
static unsigned	s_utf8_align (
	const TPU$LINE	*a_l,
	unsigned	a_col
	)
{
	while ( a_col && a_col < a_l->len && s_utf8_cont(a_l->text[a_col]) )
		a_col--;

	return	a_col;
}

/*
 * =====================================================================================
 *	Terminal: raw mode, output buffering, key decoding, screen size
 * =====================================================================================
 */
static void	s_out_flush(void);

/*
 *  DESCRIPTION: Append bytes to the output buffer, flushing it to the terminal when it is full
 *
 *  INPUTS:
 *	a_s		the bytes
 *	a_n		their count
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_out, s_outlen	the output buffer
 *
 *  RETURNS: nothing
 */
static void	s_out_putn (
	const char	*a_s,
	int		a_n
	)
{
int	l_n;

	while ( a_n > 0 )
		{
		if ( s_outlen == (int) sizeof(s_out) )
			s_out_flush();

		l_n = $MIN(a_n, (int) sizeof(s_out) - s_outlen);
		memcpy(s_out + s_outlen, a_s, l_n);
		s_outlen += l_n;
		a_s += l_n;
		a_n -= l_n;
		}
}

/*
 *  DESCRIPTION: Append a NUL-terminated string to the output buffer
 *
 *  INPUTS:
 *	a_s		the string
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_out, s_outlen	the output buffer
 *
 *  RETURNS: nothing
 */
static void	s_out_puts (
	const char	*a_s
	)
{
	s_out_putn(a_s, strlen(a_s));
}

/*
 *  DESCRIPTION: Write the output buffer to the terminal and empty it
 *
 *  IMPLICIT INPUTS:
 *	s_out, s_outlen	the output buffer
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_outlen	reset to zero
 *
 *  RETURNS: nothing
 */
static void	s_out_flush (
	void
	)
{
	if ( s_outlen )
		write(STDOUT_FILENO, s_out, s_outlen);

	s_outlen = 0;
}

/*
 *  DESCRIPTION: Queue a cursor positioning sequence
 *
 *  INPUTS:
 *	a_row		row, 0-based
 *	a_col		column, 0-based
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_out, s_outlen	the output buffer
 *
 *  RETURNS: nothing
 */
static void	s_out_goto (
	int	a_row,
	int	a_col
	)
{
char	l_buf[32];

	s_out_putn(l_buf, snprintf(l_buf, sizeof(l_buf), "\033[%d;%dH", a_row + 1, a_col + 1));
}

/*
 *  DESCRIPTION: SIGWINCH handler: only raises a flag, the size is re-read by s_key_get() on its next pass
 *
 *  INPUTS:
 *	a_sig		the signal number, ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_winch		set to 1
 *
 *  RETURNS: nothing
 */
static void	s_on_winch (
	int	a_sig
	)
{
	s_winch = 1;
}

/*
 *  DESCRIPTION: Put the terminal into raw mode, switch cursor keys and the keypad to application mode, clear
 *  the screen and install the SIGWINCH handler. The log is pointed away from the screen from here on
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_tio_saved	the original terminal settings, restored by s_term_fini()
 *	s_term_raw	set to 1
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR if standard input is not a terminal or the mode cannot be set
 */
static int	s_term_init (
	void
	)
{
struct termios	l_tio;
struct sigaction l_sa = {0};

	if ( !isatty(STDIN_FILENO) )
		return	$LOG(STS$K_ERROR, "standard input is not a terminal"), STS$K_ERROR;

	if ( tcgetattr(STDIN_FILENO, &s_tio_saved) )
		return	$LOG(STS$K_ERROR, "tcgetattr(): errno=%d, %s", errno, strerror(errno)), STS$K_ERROR;

	l_tio = s_tio_saved;
	cfmakeraw(&l_tio);
	l_tio.c_cc[VMIN] = 1;
	l_tio.c_cc[VTIME] = 0;

	if ( tcsetattr(STDIN_FILENO, TCSANOW, &l_tio) )
		return	$LOG(STS$K_ERROR, "tcsetattr(): errno=%d, %s", errno, strerror(errno)), STS$K_ERROR;

	s_term_raw = 1;
	s_log_redirect(1);

	l_sa.sa_handler = s_on_winch;				/* No SA_RESTART: read() must return EINTR	*/
	sigaction(SIGWINCH, &l_sa, NULL);

	s_out_puts("\033[?1h\033=");				/* Cursor keys and keypad in application mode	*/
	s_out_puts("\033[2J");
	s_out_flush();

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Undo s_term_init(): normal attributes, keypad numeric, cursor to the last row, saved terminal
 *  settings. Safe to call more than once. The log goes back to the standard error
 *
 *  IMPLICIT INPUTS:
 *	s_tio_saved	the original terminal settings
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_term_raw	cleared
 *
 *  RETURNS: nothing
 */
static void	s_term_fini (
	void
	)
{
	if ( !s_term_raw )
		return;

	s_out_puts("\033[0m\033[?1l\033>");
	s_out_goto(s_rows - 1, 0);
	s_out_puts("\033[2K\r\n");
	s_out_flush();

	tcsetattr(STDIN_FILENO, TCSANOW, &s_tio_saved);
	s_term_raw = 0;
	s_log_redirect(0);
}

/*
 *  DESCRIPTION: Read one byte from the terminal, with an optional timeout
 *
 *  INPUTS:
 *	a_timeout_ms	milliseconds to wait, -1 to block
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_hangup	set when read() returns end of file
 *	s_term_pushback	a byte given back by s_term_read_key(), returned first and cleared
 *
 *  RETURNS: the byte, or -1 on timeout, EINTR or end of file
 */
static int	s_term_getc (
	int	a_timeout_ms
	)
{
unsigned char	l_c;
struct pollfd	l_pfd = {STDIN_FILENO, POLLIN, 0};
int	l_rc;

	if ( s_term_pushback >= 0 )
		{
		l_rc = s_term_pushback;
		s_term_pushback = -1;
		return	l_rc;
		}

	if ( a_timeout_ms >= 0 )
		{
		if ( (l_rc = poll(&l_pfd, 1, a_timeout_ms)) <= 0 )
			return	-1;
		}

	if ( (l_rc = read(STDIN_FILENO, &l_c, 1)) == 1 )
		return	l_c;

	if ( !l_rc )
		s_hangup = 1;

	return	-1;
}

/*
 *  DESCRIPTION: Read one key from the terminal, decoding VT220/xterm escape sequences through s_keyseqs[]. A
 *  Cursor Position Report (the answer to the size query) is stored and reported as KEY_CPR
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_cpr_rows, s_cpr_cols	filled from a CPR
 *	s_term_pushback		the byte after a bare ESC that starts no sequence, read again as the next key
 *
 *  RETURNS: the key code, a bare ESC (0x1b) for a lone escape, or KEY_NONE on interrupt, timeout or an
 *  unknown sequence
 */
static int	s_term_read_key (
	void
	)
{
char	l_seq[16];
int	l_n = 0, l_c, l_len, l_prefix;

	if ( (l_c = s_term_getc(-1)) < 0 )
		return	KEY_NONE;

	if ( l_c != 0x1b )
		return	l_c;

	l_seq[l_n++] = l_c;

	for (;;)
		{
		if ( (l_c = s_term_getc(TPU$K_ESCWAIT)) < 0 )
			break;

		l_seq[l_n++] = l_c;
		l_seq[l_n] = '\0';

		/* Cursor Position Report: ESC [ rows ; cols R  - answer to our size query	*/
		if ( l_seq[1] == '[' && l_c == 'R' )
			{
			if ( sscanf(l_seq + 2, "%d;%dR", &s_cpr_rows, &s_cpr_cols) == 2 )
				return	KEY_CPR;
			break;
			}

		l_prefix = 0;

		for ( int l_i = 0; l_i < (int) $ARRSZ(s_keyseqs); l_i++ )
			{
			l_len = strlen(s_keyseqs[l_i].seq);

			if ( l_len == l_n && !memcmp(l_seq, s_keyseqs[l_i].seq, l_n) )
				return	s_keyseqs[l_i].key;

			if ( l_len > l_n && !memcmp(l_seq, s_keyseqs[l_i].seq, l_n) )
				l_prefix = 1;
			}

		/* A CPR in progress looks like ESC [ digits ; digits			*/
		if ( l_seq[1] == '[' && (isdigit(l_c) || l_c == ';') )
			l_prefix = 1;

		if ( !l_prefix || l_n >= (int) sizeof(l_seq) - 1 )
			break;
		}

	if ( l_n == 1 )
		return	0x1b;

	/* ESC and one byte that is no sequence (or a pause after it): GOLD, then that byte as a key of its own	*/
	if ( l_n == 2 )
		{
		s_term_pushback = (unsigned char) l_seq[1];
		return	0x1b;
		}

	return	KEY_NONE;
}

static void	s_scr_relayout(void);

/*
 *  DESCRIPTION: Ask the terminal about its size: TIOCGWINSZ first, then a DSR/CPR query for a real VT on a
 *  serial line, 24x80 as the last resort
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_rows, s_cols	the screen size
 *	s_cpr_rows, s_cpr_cols	scratch for the CPR answer
 *
 *  RETURNS: nothing
 */
static void	s_scr_getsize (
	void
	)
{
struct winsize	l_ws;
int	l_k;

	if ( !ioctl(STDOUT_FILENO, TIOCGWINSZ, &l_ws) && l_ws.ws_row && l_ws.ws_col )
		{
		s_rows = l_ws.ws_row;
		s_cols = l_ws.ws_col;
		return;
		}

	s_cpr_rows = s_cpr_cols = 0;
	s_out_puts("\0337\033[999;999H\033[6n\0338");		/* Save cursor, go to corner, ask, restore	*/
	s_out_flush();

	for ( int l_tries = 0; l_tries < 8 && !s_cpr_rows; l_tries++ )
		{
		if ( s_term_getc(500) < 0 )				/* Wait for ESC ...			*/
			break;

		/* We have eaten the ESC, decode the rest by hand */
		{
		char	l_seq[16] = "\033";
		int	l_n = 1;

		while ( l_n < (int) sizeof(l_seq) - 1 && (l_k = s_term_getc(200)) >= 0 )
			{
			l_seq[l_n++] = l_k;
			if ( l_k == 'R' )
				break;
			}
		l_seq[l_n] = '\0';
		sscanf(l_seq + 2, "%d;%dR", &s_cpr_rows, &s_cpr_cols);
		}
		}

	if ( s_cpr_rows > 0 && s_cpr_cols > 0 )
		{
		s_rows = s_cpr_rows;
		s_cols = s_cpr_cols;
		}
	else	s_rows = 24, s_cols = 80;
}

/*
 * =====================================================================================
 *	Screen: layout, shadow rows, rendering
 * =====================================================================================
 */
/*
 *  DESCRIPTION: (Re)allocate the shadow and the wanted screen rows for the current size; every shadow row is
 *  marked unknown so that it is rewritten
 *
 *  IMPLICIT INPUTS:
 *	s_rows, s_cols	the screen size
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_shadow, s_want	the row arrays
 *
 *  RETURNS: nothing
 */
static void	s_scr_alloc (
	void
	)
{

	if ( s_shadow )
		{
		for ( int l_r = 0; s_shadow[l_r].text; l_r++ )
			free(s_shadow[l_r].text), free(s_want[l_r].text);
		free(s_shadow), free(s_want);
		}

	s_shadow = calloc(s_rows + 1, sizeof(TPU$SROW));
	s_want = calloc(s_rows + 1, sizeof(TPU$SROW));

	for ( int l_r = 0; l_r < s_rows; l_r++ )
		{
		s_shadow[l_r].text = malloc(s_cols * 4 + 1);	/* Up to 4 bytes per column	*/
		s_want[l_r].text = malloc(s_cols * 4 + 1);
		s_shadow[l_r].rev = -1;				/* Unknown: force rewrite		*/
		memset(s_shadow[l_r].text, 0, s_cols * 4 + 1);
		}
}

/*
 *  DESCRIPTION: Forget what is on the screen and clear it: the next render rewrites every row
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_shadow	all rows marked unknown
 *	s_out		clear-screen sequence queued
 *
 *  RETURNS: nothing
 */
static void	s_scr_invalidate (
	void
	)
{

	for ( int l_r = 0; l_r < s_rows; l_r++ )
		s_shadow[l_r].rev = -1;

	s_out_puts("\033[2J");
}

/*
 *  DESCRIPTION: Divide the screen between the windows. Windows do not keep absolute heights, they share what
 *  the screen has now; the last row is the message line, each window has a status line below its text
 *
 *  IMPLICIT INPUTS:
 *	s_rows, s_nwin	the screen height and the number of windows
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_win[]		row0 and nrows of each window
 *	the shadow screen is reallocated and invalidated
 *
 *  RETURNS: nothing
 */
static void	s_scr_relayout (
	void
	)
{
int	l_avail = s_rows - 1 - (s_hints ? 1 : 0), l_h1;	/* Message line, then the hint line if on	*/

	s_scr_alloc();

	if ( s_nwin == 1 )
		{
		s_win[0].row0 = 0;
		s_win[0].nrows = $MAX(1, l_avail - 1);
		}
	else	{
		l_h1 = l_avail / 2;
		s_win[0].row0 = 0;
		s_win[0].nrows = $MAX(1, l_h1 - 1);
		s_win[1].row0 = l_h1;
		s_win[1].nrows = $MAX(1, l_avail - l_h1 - 1);
		}

	s_scr_invalidate();
}

/*
 *  DESCRIPTION: React to a terminal resize: re-read the size and lay the windows out again
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_winch		cleared
 *	s_rows, s_cols, s_win[]	updated
 *
 *  RETURNS: nothing
 */
static void	s_scr_resize (
	void
	)
{
	s_winch = 0;
	s_scr_getsize();
	s_scr_relayout();
}

/*
 *  DESCRIPTION: Display column of a byte offset in a line, TABs expanded and wide characters counted with
 *  their width. An offset past the end of the line maps one column per byte
 *
 *  INPUTS:
 *	a_l		the line
 *	a_col		the byte offset
 *
 *  RETURNS: the display column, 0-based
 */
static unsigned	s_disp_col (
	const TPU$LINE	*a_l,
	unsigned	a_col
	)
{
unsigned	l_dc = 0;
int	l_n;

	for ( unsigned l_i = 0; l_i < a_col && l_i < a_l->len; l_i += l_n )
		{
		l_n = s_utf8_len(a_l->text + l_i, a_l->len - l_i);
		l_dc = (a_l->text[l_i] == '\t') ? (l_dc / TPU$K_TABSZ + 1) * TPU$K_TABSZ : l_dc + s_utf8_width(a_l->text + l_i, l_n);
		}

	return	l_dc + (a_col > a_l->len ? a_col - a_l->len : 0);
}

/*
 *  DESCRIPTION: Display columns taken by the first a_n bytes of a NUL-terminated screen row
 *
 *  INPUTS:
 *	a_s		the row
 *	a_n		bytes to count
 *
 *  RETURNS: number of display columns
 */
static int	s_str_cols (
	const char	*a_s,
	int		a_n
	)
{
int	l_c, l_cols = 0;

	for ( int l_i = 0; l_i < a_n && a_s[l_i]; l_i += l_c )
		{
		l_c = s_utf8_len(a_s + l_i, a_n - l_i);
		l_cols += s_utf8_width(a_s + l_i, l_c);
		}

	return	l_cols;
}

/*
 *  DESCRIPTION: Render a line into a screen row: a_width columns starting at display column a_from, space
 *  padded. TABs are expanded, control characters are shown as '.', a wide character cut by an edge is
 *  replaced by spaces
 *
 *  INPUTS:
 *	a_l		the line
 *	a_from		first display column to show
 *	a_width		columns to produce
 *
 *  OUTPUTS:
 *	a_out		the rendered row, NUL-terminated
 *	must hold a_width * 4 + 1 bytes
 *
 *  RETURNS: nothing
 */
static void	s_line_render (
	const TPU$LINE	*a_l,
	unsigned	a_from,
	int		a_width,
	char		*a_out
	)
{
unsigned	l_dc = 0, l_out = 0, l_end = a_from + a_width;
int	l_n, l_w;
unsigned char	l_c;

	for ( unsigned l_i = 0; l_i < a_l->len && l_dc < l_end; l_i += l_n )
		{
		l_c = a_l->text[l_i];
		l_n = s_utf8_len(a_l->text + l_i, a_l->len - l_i);

		if ( l_c == '\t' )				/* TAB: spaces up to the next stop	*/
			{
			l_w = TPU$K_TABSZ - (l_dc % TPU$K_TABSZ);
			for ( int l_k = 0; l_k < l_w; l_k++, l_dc++ )
				if ( l_dc >= a_from && l_dc < l_end )
					a_out[l_out++] = ' ';
			continue;
			}

		l_w = (l_c < 32 || l_c == 127) ? 1 : s_utf8_width(a_l->text + l_i, l_n);

		if ( l_dc + l_w <= a_from )			/* Entirely left of the window	*/
			{
			l_dc += l_w;
			continue;
			}

		if ( l_dc < a_from || l_dc + l_w > l_end )	/* A wide char cut by an edge	*/
			{
			for ( int l_k = l_dc; l_k < (int) $MIN(l_dc + l_w, l_end); l_k++ )
				if ( l_k >= (int) a_from )
					a_out[l_out++] = ' ';
			l_dc += l_w;
			continue;
			}

		if ( l_c < 32 || l_c == 127 )
			a_out[l_out++] = '.';
		else	memcpy(a_out + l_out, a_l->text + l_i, l_n), l_out += l_n;

		l_dc += l_w;
		}

	for ( unsigned l_i = (l_dc > a_from) ? l_dc - a_from : 0; l_i < (unsigned) a_width; l_i++ )
		a_out[l_out++] = ' ';

	a_out[l_out] = '\0';
}

/*
 *  DESCRIPTION: Store a row into the wanted screen: whole characters while they fit, then spaces to the right
 *  edge
 *
 *  INPUTS:
 *	a_row		the screen row, rows outside the screen are ignored
 *	a_text		the row text
 *	a_rev		video attribute: 0 normal, 1 reverse, 2 dim reverse
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_want		the wanted screen
 *
 *  RETURNS: nothing
 */
static void	s_scr_put (
	int		a_row,
	const char	*a_text,
	int		a_rev
	)
{
int	l_i = 0, l_cols = 0, l_n, l_w, l_len = strlen(a_text);
char *	l_out;

	if ( a_row < 0 || a_row >= s_rows )
		return;

	l_out = s_want[a_row].text;

	while ( l_i < l_len )					/* Copy whole characters while they fit	*/
		{
		l_n = s_utf8_len(a_text + l_i, l_len - l_i);
		l_w = (a_rev == TPU$K_MARKATTR && a_text[l_i] == TPU$K_MARK) ? 0 : s_utf8_width(a_text + l_i, l_n);

		if ( l_cols + l_w > s_cols )
			break;

		memcpy(l_out, a_text + l_i, l_n);
		l_out += l_n;
		l_i += l_n;
		l_cols += l_w;
		}

	memset(l_out, ' ', s_cols - l_cols);
	l_out[s_cols - l_cols] = '\0';
	s_want[a_row].rev = a_rev;
}

/*
 *  DESCRIPTION: Scroll a window so that the cursor of its buffer is inside it, vertically and horizontally
 *  (horizontally in steps of 8 columns)
 *
 *  INPUTS:
 *	a_w		the window
 *
 *  OUTPUTS:
 *	a_w->top, a_w->left	adjusted
 *	the buffer's cur_line is clamped to the buffer
 *
 *  RETURNS: nothing
 */
static void	s_win_ensure_visible (
	TPU$WINDOW	*a_w
	)
{
TPU$BUFFER *l_b = a_w->buf;
unsigned l_dc;

	if ( l_b->cur_line >= l_b->nlines )
		l_b->cur_line = l_b->nlines - 1;

	if ( l_b->cur_line < a_w->top )
		a_w->top = l_b->cur_line;
	else if ( l_b->cur_line >= a_w->top + a_w->nrows )
		a_w->top = l_b->cur_line - a_w->nrows + 1;

	l_dc = s_disp_col(l_b->lines[l_b->cur_line], l_b->cur_col);

	if ( l_dc < a_w->left )
		a_w->left = (l_dc / 8) * 8;
	else if ( l_dc >= a_w->left + s_cols )
		a_w->left = l_dc - s_cols + 8;
}

/*
 *  DESCRIPTION: Put TPU$K_MARK bytes around the display columns [a_from, a_to) of a rendered row, so that the
 *  span is drawn in reverse video (the row then goes to s_scr_put() with TPU$K_MARKATTR)
 *
 *  INPUTS:
 *	a_from, a_to	display columns, 0 <= a_from < a_to <= width of the row
 *
 *  OUTPUTS:
 *	a_row		the row, NUL-terminated, two bytes longer; must have room for them
 *
 *  RETURNS: nothing
 */
static void	s_row_mark (
	char	*a_row,
	int	a_from,
	int	a_to
	)
{
int	l_len = strlen(a_row), l_col = 0, l_n, l_i = 0, l_pa = -1, l_pz = l_len;

	for ( l_i = 0; l_i < l_len; l_i += l_n )	/* Byte offsets of the two columns		*/
		{
		if ( l_col >= a_from && l_pa < 0 )
			l_pa = l_i;
		if ( l_col >= a_to )
			{
			l_pz = l_i;
			break;
			}

		l_n = s_utf8_len(a_row + l_i, l_len - l_i);
		l_col += s_utf8_width(a_row + l_i, l_n);
		}

	if ( l_pa < 0 )
		l_pa = l_len;

	memmove(a_row + l_pz + 1, a_row + l_pz, l_len - l_pz + 1);
	a_row[l_pz] = TPU$K_MARK;
	memmove(a_row + l_pa + 1, a_row + l_pa, l_len + 1 - l_pa + 1);
	a_row[l_pa] = TPU$K_MARK;
}

/*
 *  DESCRIPTION: Render a window's text rows and its status line into the wanted screen
 *
 *  INPUTS:
 *	a_w		the window
 *	a_current	non-zero for the window with the cursor, its status line is brighter
 *
 *  IMPLICIT INPUTS:
 *	s_learning	adds the "Learning" tag to the status line
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_want		the wanted screen
 *
 *  RETURNS: nothing
 */
static void	s_win_render (
	TPU$WINDOW	*a_w,
	int		a_current
	)
{
TPU$BUFFER *l_b = a_w->buf;
TPU$POS	l_from, l_to;
char	l_row[s_cols * 4 + 64];
unsigned l_idx;
int	l_sel = s_sel_bounds(l_b, &l_from, &l_to), l_a, l_z, l_attr;

	s_win_ensure_visible(a_w);

	for ( int l_r = 0; l_r < a_w->nrows; l_r++ )
		{
		l_idx = a_w->top + l_r;
		l_attr = 0;

		if ( l_idx < l_b->nlines )
			{
			s_line_render(l_b->lines[l_idx], a_w->left, s_cols, l_row);

			/* Selected span of this line in window columns; a newline inside the selection is one cell */
			if ( l_sel && l_idx >= l_from.line && l_idx <= l_to.line )
				{
				l_a = (l_idx == l_from.line) ? (int) s_disp_col(l_b->lines[l_idx], l_from.col) : 0;
				l_z = (l_idx == l_to.line) ? (int) s_disp_col(l_b->lines[l_idx], l_to.col) : (int) s_disp_col(l_b->lines[l_idx], l_b->lines[l_idx]->len) + 1;
				l_a = $MAX(l_a - (int) a_w->left, 0);
				l_z = $MIN(l_z - (int) a_w->left, s_cols);

				if ( l_a < l_z )
					s_row_mark(l_row, l_a, l_z), l_attr = TPU$K_MARKATTR;
				}
			}
		else if ( l_idx == l_b->nlines )
			snprintf(l_row, sizeof(l_row), "[End of file]");
		else	l_row[0] = '\0';

		s_scr_put(a_w->row0 + l_r, l_row, l_attr);
		}

	snprintf(l_row, sizeof(l_row), " Buffer: %s%s%*s| %s | %s | Line %u of %u ",
		l_b->name, (l_b->flags & BUF$M_MODIFIED) ? "*" : "",
		(int) $MAX(1, 24 - (int) strlen(l_b->name)), "",
		l_b->overstrike ? "Overstrike" : "Insert",
		l_b->reverse ? "Reverse" : "Forward",
		l_b->cur_line + 1, l_b->nlines);

	if ( s_learning )
		strncat(l_row, "| Learning ", sizeof(l_row) - strlen(l_row) - 1);

	s_scr_put(a_w->row0 + a_w->nrows, l_row, a_current ? 1 : 2);
}

/* The commands worth a hint, in order of importance, with the key they usually sit on */
static const struct { const char *cmd; int key; } s_hintcmds[] = {
	{"DO", KEY_KPMINUS},		{"EXIT", 0x1a},			{"HELP", KEY_PF1},		{"SHOW KEYS", TPU$M_GOLD | 'k'},
	{"FIND", KEY_FIND},		{"REPLACE", TPU$M_GOLD | 'r'},	{"SELECT", KEY_SELECT},		{"REMOVE", KEY_REMOVE},
	{"INSERT HERE", KEY_INSERT},	{"GET FILE", TPU$M_GOLD | 'g'},	{"WRITE FILE", TPU$M_GOLD | 'w'}, {"BUFFER", TPU$M_GOLD | 'b'},
	{"QUIT", TPU$M_GOLD | 'q'},	{"LEARN", 0x0b},		{"TWO WINDOWS", TPU$M_GOLD | 't'},
};

/*
 *  DESCRIPTION: Build the key hint row, nano style: "KP- DO  ^Z EXIT  ...", the key between TPU$K_MARK
 *  bytes so that it is drawn in reverse video. Keys come from the live table: the usual key of a command is
 *  shown while it is still bound to it, otherwise the first key that is; a command with no key is left out.
 *  As many hints as fit the screen width are taken, in the order of s_hintcmds[]
 *
 *  INPUTS:
 *	a_sz		size of a_out
 *
 *  OUTPUTS:
 *	a_out		the row, NUL-terminated
 *
 *  IMPLICIT INPUTS:
 *	s_keydefs, s_nkeydefs, s_cols
 *
 *  RETURNS: nothing
 */
static void	s_hint_row (
	char	*a_out,
	int	a_sz
	)
{
TPU$KEYDEF *l_kd;
char	l_name[32], *l_p;
int	l_key, l_len = 0, l_cols = 0, l_w;

	a_out[0] = '\0';

	for ( int l_i = 0; l_i < (int) $ARRSZ(s_hintcmds); l_i++ )
		{
		l_key = KEY_NONE;
		l_kd = s_keydef_find(s_hintcmds[l_i].key);

		if ( l_kd && l_kd->type == KD$K_CMD && !strcmp(l_kd->cmd->name, s_hintcmds[l_i].cmd) )
			l_key = l_kd->key;
		else	for ( int l_k = 0; l_k < s_nkeydefs; l_k++ )
				if ( s_keydefs[l_k].type == KD$K_CMD && !strcmp(s_keydefs[l_k].cmd->name, s_hintcmds[l_i].cmd) )
					{
					l_key = s_keydefs[l_k].key;
					break;
					}

		if ( l_key == KEY_NONE )
			continue;

		s_key_name(l_key, l_name, sizeof(l_name));

		if ( !strncmp(l_name, "Ctrl-", 5) )		/* ^Z is shorter and everybody reads it	*/
			memmove(l_name + 1, l_name + 5, strlen(l_name + 5) + 1), l_name[0] = '^';

		l_w = strlen(l_name) + 1 + strlen(s_hintcmds[l_i].cmd);

		if ( l_cols + (l_cols ? 2 : 0) + l_w > s_cols || l_len + l_w + 5 >= a_sz )
			break;

		l_p = a_out + l_len;
		l_len += snprintf(l_p, a_sz - l_len, "%s%c%s%c %s", l_cols ? "  " : "", TPU$K_MARK, l_name, TPU$K_MARK, s_hintcmds[l_i].cmd);
		l_cols += (l_cols ? 2 : 0) + l_w;
		}
}

/*
 *  DESCRIPTION: Draw everything: windows, the message line (or the active prompt), then emit only the spans
 *  that differ from the shadow screen and place the cursor. A pending resize is handled first
 *
 *  IMPLICIT INPUTS:
 *	s_win[], s_nwin, s_curwin, s_msg, s_prompt_label, s_prompt_text
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_shadow	brought up to date with s_want
 *	the output buffer is flushed
 *
 *  RETURNS: nothing
 */
static void	s_scr_render (
	void
	)
{
int	l_f, l_l, l_crow, l_ccol, l_n;
char	l_row[s_cols * 4 + 64];
TPU$BUFFER *l_b;

	if ( s_winch )
		s_scr_resize();

	if ( s_rows < 4 || s_cols < 20 )
		{
		s_out_puts("\033[2J\033[H Screen too small");
		s_out_flush();
		return;
		}

	for ( int l_w = 0; l_w < s_nwin; l_w++ )
		s_win_render(&s_win[l_w], l_w == s_curwin);

	/* Message line: a prompt, or the last message */
	if ( s_prompt_label )
		{
		snprintf(l_row, sizeof(l_row), "%s%s", s_prompt_label, s_prompt_text);
		l_crow = s_rows - 1;
		l_ccol = $MIN(s_str_cols(l_row, strlen(l_row)), s_cols - 1);
		}
	else	{
		snprintf(l_row, sizeof(l_row), "%s", s_msg);
		l_b = CURBUF;
		l_crow = CURWIN->row0 + (l_b->cur_line - CURWIN->top);
		l_ccol = s_disp_col(l_b->lines[l_b->cur_line], l_b->cur_col) - CURWIN->left;
		}

	s_scr_put(s_rows - 1, l_row, 0);

	if ( s_hints )
		{
		s_hint_row(l_row, sizeof(l_row));
		s_scr_put(s_rows - 2, l_row, TPU$K_MARKATTR);
		}

	/* Emit only what differs from the shadow */
	for ( int l_r = 0; l_r < s_rows; l_r++ )
		{
		const char *l_sh = s_shadow[l_r].text, *l_wt = s_want[l_r].text;
		int	l_wlen = strlen(l_wt);

		if ( s_shadow[l_r].rev != s_want[l_r].rev )
			{
			l_f = 0;
			l_l = l_wlen;
			}
		else	{
			if ( !strcmp(l_sh, l_wt) )
				continue;

			/* First and last differing bytes, widened to whole characters and whole
			 * trailing tail: a byte-shift in the middle of a row makes the tail differ anyway */
			for ( l_f = 0; l_sh[l_f] && l_sh[l_f] == l_wt[l_f]; l_f++ );
			while ( l_f && s_utf8_cont(l_wt[l_f]) )
				l_f--;
			l_l = l_wlen;
			}

		if ( s_want[l_r].rev == TPU$K_MARKATTR )		/* Marked row: whole row, marks toggle reverse video	*/
			{
			int	l_on = 0;

			s_out_goto(l_r, 0);
			s_out_puts("\033[0m");

			for ( const char *l_p = l_wt; *l_p; l_p++ )
				if ( *l_p == TPU$K_MARK )
					s_out_puts((l_on = !l_on) ? "\033[7m" : "\033[0m");
				else	s_out_putn(l_p, 1);

			s_out_puts("\033[0m");			/* The row is padded to the width, no [K needed	*/
			strcpy(s_shadow[l_r].text, l_wt);
			s_shadow[l_r].rev = s_want[l_r].rev;
			continue;
			}

		s_out_goto(l_r, s_str_cols(l_wt, l_f));
		s_out_puts(s_want[l_r].rev == 1 ? "\033[7m" : s_want[l_r].rev == 2 ? "\033[7;2m" : "\033[0m");
		s_out_putn(l_wt + l_f, l_l - l_f);
		if ( (l_n = s_cols - s_str_cols(l_wt, l_wlen)) > 0 )
			s_out_puts("\033[K");
		s_out_puts("\033[0m");

		strcpy(s_shadow[l_r].text, l_wt);
		s_shadow[l_r].rev = s_want[l_r].rev;
		}

	s_out_goto(l_crow, l_ccol);
	s_out_flush();
}

/*
 * =====================================================================================
 *	Messages: the catalogue from tpumsg.h is registered with StarLet, s_tpu_msg() looks the
 *	text up by condition code, formats it onto the message line and into the MESSAGES
 *	buffer, and returns the code, so "return s_tpu_msg(TPU$_NOTFOUND, ...)" is the idiom
 *	for a failing command: the low three bits carry the severity.
 * =====================================================================================
 */
static EMSG_RECORD	s_msgrecs[] = {
#define	X(n, l, t)	{TPU$_##n, {$ASCINI("%TPU-" #l "-" #n ", " t)}},
	TPU$MESSAGES
#undef	X
};

static EMSG_RECORD_DESC	s_msgdesc = {NULL, $ARRSZ(s_msgrecs), TPU$_FACILITY, s_msgrecs};

/*
 *  DESCRIPTION: Format a message from the catalogue onto the message line and into the MESSAGES buffer, and
 *  return its condition code, so that "return s_tpu_msg(TPU$_NOTFOUND, ...)" is the idiom for a failing
 *  command. An unknown code is reported through TPU$_NOMSG
 *
 *  INPUTS:
 *	a_code		a TPU$_ condition code
 *	...		arguments for the message's format
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_msg		the message line
 *	s_msgbuf	the MESSAGES buffer gets a copy
 *
 *  RETURNS: a_code, whose low three bits carry the severity
 */
static int	s_tpu_msg (
	unsigned	a_code,
	...
	)
{
va_list	l_ap;
int	l_len;
EMSG_RECORD *l_rec;

	if ( !(1 & __util$getmsg(a_code, &l_rec)) )
		{
		__util$getmsg(TPU$_NOMSG, &l_rec);
		l_len = snprintf(s_msg, sizeof(s_msg), l_rec->text, a_code);
		}
	else	{
		va_start(l_ap, a_code);
		l_len = vsnprintf(s_msg, sizeof(s_msg), l_rec->text, l_ap);
		va_end(l_ap);
		}

	l_len = $MIN(l_len, (int) sizeof(s_msg) - 1);

	if ( s_msgbuf )
		s_buf_append_line(s_msgbuf, s_msg, l_len);

	$TRACE("%s", s_msg);

	return	a_code;
}

/*
 * =====================================================================================
 *	Lines
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Allocate a line holding a copy of the given bytes
 *
 *  INPUTS:
 *	a_text		the bytes, may be NULL when a_len is 0
 *	a_len		their count
 *
 *  RETURNS: the new line
 */
static TPU$LINE *	s_line_new (
	const char	*a_text,
	unsigned	a_len
	)
{
TPU$LINE *	l_l = calloc(1, sizeof(TPU$LINE));

	l_l->alloc = $MAX(a_len + 1, 16);
	l_l->text = malloc(l_l->alloc);
	l_l->len = a_len;

	if ( a_len )
		memcpy(l_l->text, a_text, a_len);

	return	l_l;
}

/*
 *  DESCRIPTION: Release a line and its text
 *
 *  INPUTS:
 *	a_l		the line
 *
 *  RETURNS: nothing
 */
static void	s_line_free (
	TPU$LINE	*a_l
	)
{
	free(a_l->text);
	free(a_l);
}

/*
 *  DESCRIPTION: Insert bytes into a line at a byte offset, growing the line as needed; an offset past the end
 *  inserts at the end
 *
 *  INPUTS:
 *	a_l		the line
 *	a_at		byte offset
 *	a_text		the bytes
 *	a_len		their count
 *
 *  OUTPUTS:
 *	a_l		modified
 *
 *  RETURNS: nothing
 */
static void	s_line_ins (
	TPU$LINE	*a_l,
	unsigned	a_at,
	const char	*a_text,
	unsigned	a_len
	)
{
	if ( a_at > a_l->len )
		a_at = a_l->len;

	if ( a_l->len + a_len + 1 > a_l->alloc )
		{
		a_l->alloc = (a_l->len + a_len + 1) * 3 / 2;
		a_l->text = realloc(a_l->text, a_l->alloc);
		}

	memmove(a_l->text + a_at + a_len, a_l->text + a_at, a_l->len - a_at);
	memcpy(a_l->text + a_at, a_text, a_len);
	a_l->len += a_len;
}

/*
 *  DESCRIPTION: Delete bytes from a line; the count is clipped to what is there
 *
 *  INPUTS:
 *	a_l		the line
 *	a_at		byte offset, past the end - nothing happens
 *	a_len		bytes to delete
 *
 *  OUTPUTS:
 *	a_l		modified
 *
 *  RETURNS: nothing
 */
static void	s_line_del (
	TPU$LINE	*a_l,
	unsigned	a_at,
	unsigned	a_len
	)
{
	if ( a_at >= a_l->len )
		return;

	a_len = $MIN(a_len, a_l->len - a_at);
	memmove(a_l->text + a_at, a_l->text + a_at + a_len, a_l->len - a_at - a_len);
	a_l->len -= a_len;
}

/*
 * =====================================================================================
 *	Buffers
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Insert a line into a buffer at an index, growing the line array as needed; an index past the
 *  end appends
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_idx		the index
 *	a_l		the line, owned by the buffer from now on
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: nothing
 */
static void	s_buf_ins_line (
	TPU$BUFFER	*a_b,
	unsigned	a_idx,
	TPU$LINE	*a_l
	)
{
	if ( a_b->nlines + 1 > a_b->alloc )
		{
		a_b->alloc = $MAX(a_b->alloc * 2, 64);
		a_b->lines = realloc(a_b->lines, a_b->alloc * sizeof(TPU$LINE *));
		}

	if ( a_idx > a_b->nlines )
		a_idx = a_b->nlines;

	memmove(a_b->lines + a_idx + 1, a_b->lines + a_idx, (a_b->nlines - a_idx) * sizeof(TPU$LINE *));
	a_b->lines[a_idx] = a_l;
	a_b->nlines++;
}

/*
 *  DESCRIPTION: Delete a line from a buffer, keeping the invariant that a buffer has at least one (possibly
 *  empty) line
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_idx		the index, out of range - nothing happens
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: nothing
 */
static void	s_buf_del_line (
	TPU$BUFFER	*a_b,
	unsigned	a_idx
	)
{
	if ( a_idx >= a_b->nlines )
		return;

	if ( a_b->nlines == 1 )
		{
		a_b->lines[0]->len = 0;
		return;
		}

	s_line_free(a_b->lines[a_idx]);
	memmove(a_b->lines + a_idx, a_b->lines + a_idx + 1, (a_b->nlines - a_idx - 1) * sizeof(TPU$LINE *));
	a_b->nlines--;
}

/*
 *  DESCRIPTION: Drop all the text of a buffer, leaving one empty line, the cursor at the top and no selection
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: nothing
 */
static void	s_buf_clear (
	TPU$BUFFER	*a_b
	)
{

	for ( unsigned l_i = 0; l_i < a_b->nlines; l_i++ )
		s_line_free(a_b->lines[l_i]);

	a_b->nlines = 0;
	s_buf_ins_line(a_b, 0, s_line_new("", 0));
	a_b->cur_line = a_b->cur_col = 0;
	a_b->mark_line = a_b->mark_col = -1;
}

/*
 *  DESCRIPTION: Look a buffer up by name, case-insensitively
 *
 *  INPUTS:
 *	a_name		the name
 *
 *  IMPLICIT INPUTS:
 *	s_buffers	the registry
 *
 *  RETURNS: the buffer, or NULL
 */
static TPU$BUFFER *	s_buf_find (
	const char	*a_name
	)
{
TPU$BUFFER *l_b;

	for ( l_b = s_buffers; l_b; l_b = l_b->next )
		if ( !strcasecmp(l_b->name, a_name) )
			return	l_b;

	return	NULL;
}

/*
 *  DESCRIPTION: Create an empty buffer and register it; buffers are kept in creation order. The name is not
 *  checked for uniqueness, that is the caller's business
 *
 *  INPUTS:
 *	a_name		the name, truncated to fit
 *	a_flags		BUF$M_ flags
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_buffers	the registry
 *
 *  RETURNS: the new buffer
 */
static TPU$BUFFER *	s_buf_create (
	const char	*a_name,
	unsigned	a_flags
	)
{
TPU$BUFFER *l_b = calloc(1, sizeof(TPU$BUFFER)), **l_pp;

	snprintf(l_b->name, sizeof(l_b->name), "%s", a_name);
	l_b->flags = a_flags;
	l_b->mark_line = l_b->mark_col = -1;
	s_buf_ins_line(l_b, 0, s_line_new("", 0));

	for ( l_pp = &s_buffers; *l_pp; l_pp = &(*l_pp)->next );	/* Append: keep creation order	*/
	*l_pp = l_b;

	$TRACE("Buffer '%s' created, flags=%#x", a_name, a_flags);

	return	l_b;
}

/*
 *  DESCRIPTION: Unregister a buffer and release its lines. Windows showing it are the caller's business
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_buffers	the registry
 *
 *  RETURNS: nothing
 */
static void	s_buf_destroy (
	TPU$BUFFER	*a_b
	)
{
TPU$BUFFER **l_pp;

	for ( l_pp = &s_buffers; *l_pp && *l_pp != a_b; l_pp = &(*l_pp)->next );
	if ( *l_pp )
		*l_pp = a_b->next;

	for ( unsigned l_i = 0; l_i < a_b->nlines; l_i++ )
		s_line_free(a_b->lines[l_i]);

	free(a_b->lines);
	free(a_b);
}

/*
 *  DESCRIPTION: Append a line at the end of a buffer; a buffer holding nothing but one untouched empty line
 *  has that line filled instead of a second one added
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_text		the bytes
 *	a_len		their count
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_buf_append_line (
	TPU$BUFFER	*a_b,
	const char	*a_text,
	unsigned	a_len
	)
{
	if ( a_b->nlines == 1 && !a_b->lines[0]->len )
		s_line_ins(a_b->lines[0], 0, a_text, a_len);
	else	s_buf_ins_line(a_b, a_b->nlines, s_line_new(a_text, a_len));

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Replace a buffer's content with a file and tie the buffer to it. A missing file is not an
 *  error: the buffer stays empty and the file is created on write. A file ending with a newline gets an empty
 *  last line, which s_buf_write() does not write back, so the file round-trips unchanged
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_file		the file name
 *
 *  OUTPUTS:
 *	a_b		content, file name, MODIFIED flag cleared
 *
 *  RETURNS: TPU$_READ, TPU$_NEWFILE, or TPU$_OPENIN
 */
static int	s_buf_load (
	TPU$BUFFER	*a_b,
	const char	*a_file
	)
{
FILE *	l_f;
char *	l_line = NULL;
size_t	l_cap = 0;
ssize_t	l_n;
unsigned l_count = 0;
int	l_nl = 0;

	s_buf_clear(a_b);
	snprintf(a_b->file, sizeof(a_b->file), "%s", a_file);
	a_b->flags &= ~BUF$M_MODIFIED;

	if ( !(l_f = fopen(a_file, "r")) )
		{
		if ( errno == ENOENT )
			return	s_tpu_msg(TPU$_NEWFILE, a_file);

		return	s_tpu_msg(TPU$_OPENIN, a_file, strerror(errno));
		}

	s_line_free(a_b->lines[0]);				/* Drop the placeholder line	*/
	a_b->nlines = 0;

	while ( (l_n = getline(&l_line, &l_cap, l_f)) > 0 )
		{
		if ( (l_nl = (l_line[l_n - 1] == '\n')) )
			l_n--;

		s_buf_ins_line(a_b, a_b->nlines, s_line_new(l_line, l_n));
		l_count++;
		}

	free(l_line);
	fclose(l_f);

	/* A file ending with a newline gets an empty last line: the cursor can stand after the last
	 * newline, and s_buf_write() does not write that line back, so the file round-trips unchanged. */
	if ( !a_b->nlines || l_nl )
		s_buf_ins_line(a_b, a_b->nlines, s_line_new("", 0));

	return	s_tpu_msg(TPU$_READ, l_count, a_file);
}

/*
 *  DESCRIPTION: Write a buffer to a file, one record per line, and tie the buffer to that file. A trailing
 *  empty line is not a record
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_file		the file name
 *
 *  OUTPUTS:
 *	a_b		file name set, MODIFIED flag cleared
 *
 *  RETURNS: TPU$_WRITTEN, TPU$_OPENOUT or TPU$_WRITEERR
 */
static int	s_buf_write (
	TPU$BUFFER	*a_b,
	const char	*a_file
	)
{
FILE *	l_f;
unsigned	l_n;

	if ( !(l_f = fopen(a_file, "w")) )
		return	s_tpu_msg(TPU$_OPENOUT, a_file, strerror(errno));

	l_n = a_b->nlines;

	if ( !a_b->lines[l_n - 1]->len )			/* Trailing empty line is not a record	*/
		l_n--;

	for ( unsigned l_i = 0; l_i < l_n; l_i++ )
		{
		fwrite(a_b->lines[l_i]->text, 1, a_b->lines[l_i]->len, l_f);
		fputc('\n', l_f);
		}

	if ( fclose(l_f) )
		return	s_tpu_msg(TPU$_WRITEERR, a_file, strerror(errno));

	if ( a_file != a_b->file )				/* EXIT passes the buffer's own name	*/
		snprintf(a_b->file, sizeof(a_b->file), "%s", a_file);

	a_b->flags &= ~BUF$M_MODIFIED;

	return	s_tpu_msg(TPU$_WRITTEN, l_n, a_file);
}

/*
 *  DESCRIPTION: Which buffer to show when the given one goes away: the first user buffer that is not it, or a
 *  fresh MAIN
 *
 *  INPUTS:
 *	a_not		the buffer going away
 *
 *  IMPLICIT INPUTS:
 *	s_buffers	the registry
 *
 *  RETURNS: a buffer to show
 */
static TPU$BUFFER *	s_buf_fallback (
	TPU$BUFFER	*a_not
	)
{
TPU$BUFFER *l_b;

	for ( l_b = s_buffers; l_b; l_b = l_b->next )
		if ( l_b != a_not && !(l_b->flags & BUF$M_SYSTEM) )
			return	l_b;

	return	s_buf_create("MAIN", 0);
}

#define	CURLINE(b)	((b)->lines[(b)->cur_line])

/*
 *  DESCRIPTION: May the user change this buffer?
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_READONLY (the message is shown)
 */
static int	s_buf_writable (
	TPU$BUFFER	*a_b
	)
{
	if ( a_b->flags & BUF$M_NOWRITE )
		return	s_tpu_msg(TPU$_READONLY, a_b->name);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Mark a buffer as modified
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b->flags	BUF$M_MODIFIED set
 *
 *  RETURNS: nothing
 */
static void	s_buf_touch (
	TPU$BUFFER	*a_b
	)
{
	a_b->flags |= BUF$M_MODIFIED;
}

/*
 * =====================================================================================
 *	Ranges: [a, b) in text order, the newline of a line is at col == len
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Compare two positions in text order
 *
 *  INPUTS:
 *	a_a, a_b	the positions
 *
 *  RETURNS: -1, 0 or 1
 */
static int	s_pos_cmp (
	TPU$POS	a_a,
	TPU$POS	a_b
	)
{
	if ( a_a.line != a_b.line )
		return	a_a.line < a_b.line ? -1 : 1;

	return	a_a.col < a_b.col ? -1 : a_a.col > a_b.col ? 1 : 0;
}

/*
 *  DESCRIPTION: Copy the range [a_a, a_b) of a buffer into another buffer, replacing its content. The newline
 *  of a line is at col == len
 *
 *  INPUTS:
 *	a_src		the source
 *	a_a, a_b	the range, a_a <= a_b
 *	a_dst		the destination, cleared first
 *
 *  OUTPUTS:
 *	a_dst		the copy
 *
 *  RETURNS: nothing
 */
static void	s_range_copy (
	TPU$BUFFER	*a_src,
	TPU$POS		a_a,
	TPU$POS		a_b,
	TPU$BUFFER	*a_dst
	)
{
TPU$LINE *	l_l;

	s_buf_clear(a_dst);
	a_dst->nlines = 0;

	if ( a_a.line == a_b.line )
		{
		l_l = a_src->lines[a_a.line];
		s_buf_ins_line(a_dst, 0, s_line_new(l_l->text + a_a.col, a_b.col - a_a.col));
		return;
		}

	l_l = a_src->lines[a_a.line];
	s_buf_ins_line(a_dst, a_dst->nlines, s_line_new(l_l->text + a_a.col, l_l->len - a_a.col));

	for ( unsigned l_i = a_a.line + 1; l_i < a_b.line; l_i++ )
		s_buf_ins_line(a_dst, a_dst->nlines, s_line_new(a_src->lines[l_i]->text, a_src->lines[l_i]->len));

	l_l = a_src->lines[a_b.line];
	s_buf_ins_line(a_dst, a_dst->nlines, s_line_new(l_l->text, a_b.col));
}

/*
 *  DESCRIPTION: Delete the range [a_a, a_b2) from a buffer; the cursor goes to the start of the range and the
 *  selection is cancelled
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_a, a_b2	the range, a_a <= a_b2
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: nothing
 */
static void	s_range_delete (
	TPU$BUFFER	*a_b,
	TPU$POS		a_a,
	TPU$POS		a_b2
	)
{
TPU$LINE *	l_la, *l_lb;

	if ( a_a.line == a_b2.line )
		s_line_del(a_b->lines[a_a.line], a_a.col, a_b2.col - a_a.col);
	else	{
		l_la = a_b->lines[a_a.line];
		l_lb = a_b->lines[a_b2.line];
		l_la->len = $MIN(l_la->len, a_a.col);
		s_line_ins(l_la, l_la->len, l_lb->text + a_b2.col, l_lb->len - a_b2.col);

		for ( unsigned l_i = a_b2.line; l_i > a_a.line; l_i-- )
			s_buf_del_line(a_b, l_i);
		}

	a_b->cur_line = a_a.line;
	a_b->cur_col = a_a.col;
	a_b->mark_line = a_b->mark_col = -1;
	s_buf_touch(a_b);
}

/*
 *  DESCRIPTION: Insert the content of one buffer at the cursor of another; the cursor ends after the inserted
 *  text. A one-line clip goes into the current line, a longer one splits it
 *
 *  INPUTS:
 *	a_b		the destination
 *	a_clip		the text to insert
 *
 *  OUTPUTS:
 *	a_b		modified, cursor moved
 *
 *  RETURNS: nothing
 */
static void	s_clip_insert (
	TPU$BUFFER	*a_b,
	TPU$BUFFER	*a_clip
	)
{
TPU$LINE *	l_cur = CURLINE(a_b), *l_new;
char *	l_tail;
unsigned	l_taillen;

	if ( a_clip->nlines == 1 )
		{
		s_line_ins(l_cur, a_b->cur_col, a_clip->lines[0]->text, a_clip->lines[0]->len);
		a_b->cur_col += a_clip->lines[0]->len;
		s_buf_touch(a_b);
		return;
		}

	a_b->cur_col = $MIN(a_b->cur_col, l_cur->len);
	l_taillen = l_cur->len - a_b->cur_col;
	l_tail = malloc(l_taillen + 1);
	memcpy(l_tail, l_cur->text + a_b->cur_col, l_taillen);

	l_cur->len = a_b->cur_col;
	s_line_ins(l_cur, l_cur->len, a_clip->lines[0]->text, a_clip->lines[0]->len);

	for ( unsigned l_i = 1; l_i < a_clip->nlines; l_i++ )
		{
		l_new = s_line_new(a_clip->lines[l_i]->text, a_clip->lines[l_i]->len);
		s_buf_ins_line(a_b, a_b->cur_line + l_i, l_new);
		}

	a_b->cur_line += a_clip->nlines - 1;
	a_b->cur_col = CURLINE(a_b)->len;
	s_line_ins(CURLINE(a_b), a_b->cur_col, l_tail, l_taillen);

	free(l_tail);
	s_buf_touch(a_b);
}

/*
 *  DESCRIPTION: The selection as an ordered range [a_from, a_to), anchor and cursor in text order, without
 *  any message: the renderer asks on every redraw
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_from, a_to	the range, a_from <= a_to; untouched when there is no selection
 *
 *  RETURNS: 1 when a selection is active, 0 otherwise
 */
static int	s_sel_bounds (
	TPU$BUFFER	*a_b,
	TPU$POS		*a_from,
	TPU$POS		*a_to
	)
{
TPU$POS	l_m, l_c;

	if ( a_b->mark_line < 0 )
		return	0;

	l_m.line = a_b->mark_line;
	l_m.col = a_b->mark_col;
	l_c.line = a_b->cur_line;
	l_c.col = $MIN(a_b->cur_col, CURLINE(a_b)->len);

	if ( s_pos_cmp(l_m, l_c) <= 0 )
		*a_from = l_m, *a_to = l_c;
	else	*a_from = l_c, *a_to = l_m;

	return	1;
}

/*
 *  DESCRIPTION: The current selection as an ordered range, from the SELECT anchor to the cursor or the other
 *  way round
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_from, a_to	the range, a_from <= a_to
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOSELECT or TPU$_EMPTYSEL
 */
static int	s_sel_range (
	TPU$BUFFER	*a_b,
	TPU$POS		*a_from,
	TPU$POS		*a_to
	)
{
	if ( !s_sel_bounds(a_b, a_from, a_to) )
		return	s_tpu_msg(TPU$_NOSELECT);

	if ( !s_pos_cmp(*a_from, *a_to) )
		return	s_tpu_msg(TPU$_EMPTYSEL);

	return	STS$K_SUCCESS;
}

/*
 * =====================================================================================
 *	Editing primitives
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Insert one byte at the cursor, or overstrike the character there. A cursor past the end of
 *  the line pads it with spaces first. In overstrike mode a multibyte character arrives byte by byte: the
 *  lead byte replaces the old character, the continuation bytes are simply inserted after it
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_c		the byte
 *
 *  OUTPUTS:
 *	a_b		modified, cursor advanced one byte
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_insert_char (
	TPU$BUFFER	*a_b,
	int		a_c
	)
{
TPU$LINE *	l_l = CURLINE(a_b);
char	l_c = a_c;

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	while ( a_b->cur_col > l_l->len )			/* Free cursor: pad with spaces		*/
		s_line_ins(l_l, l_l->len, " ", 1);

	/* Overstrike replaces a whole character; a multibyte one arrives byte by byte, the lead
	 * byte does the replacing and the continuation bytes are simply inserted after it */
	if ( a_b->overstrike && a_b->cur_col < l_l->len && !s_utf8_cont(l_c) )
		s_line_del(l_l, a_b->cur_col, s_utf8_len(l_l->text + a_b->cur_col, l_l->len - a_b->cur_col));

	s_line_ins(l_l, a_b->cur_col, &l_c, 1);

	a_b->cur_col++;
	s_buf_touch(a_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Split the current line at the cursor; the cursor goes to the start of the new line
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_newline (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);
unsigned l_col = $MIN(a_b->cur_col, l_l->len);

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	s_buf_ins_line(a_b, a_b->cur_line + 1, s_line_new(l_l->text + l_col, l_l->len - l_col));
	l_l->len = l_col;
	a_b->cur_line++;
	a_b->cur_col = 0;
	s_buf_touch(a_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Keep erased text for RESTORE
 *
 *  INPUTS:
 *	a_text		the bytes
 *	a_len		their count
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_restorebuf	replaced
 *
 *  RETURNS: nothing
 */
static void	s_ed_save_restore (
	const char	*a_text,
	unsigned	a_len
	)
{
	s_buf_clear(s_restorebuf);
	s_line_ins(s_restorebuf->lines[0], 0, a_text, a_len);
}

/*
 *  DESCRIPTION: Erase the character before the cursor; at the start of a line join it with the previous one.
 *  A cursor past the end of the line just steps back
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_erase_prev (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b), *l_p;

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	if ( a_b->cur_col > l_l->len )
		{
		a_b->cur_col--;
		return	STS$K_SUCCESS;
		}

	if ( a_b->cur_col )
		{
		a_b->cur_col = s_utf8_align(l_l, a_b->cur_col - 1);
		s_line_del(l_l, a_b->cur_col, s_utf8_len(l_l->text + a_b->cur_col, l_l->len - a_b->cur_col));
		s_buf_touch(a_b);
		return	STS$K_SUCCESS;
		}

	if ( !a_b->cur_line )
		return	STS$K_SUCCESS;

	l_p = a_b->lines[a_b->cur_line - 1];			/* Join with the previous line		*/
	a_b->cur_col = l_p->len;
	s_line_ins(l_p, l_p->len, l_l->text, l_l->len);
	s_buf_del_line(a_b, a_b->cur_line);
	a_b->cur_line--;
	s_buf_touch(a_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Erase the character under the cursor; at the end of a line join the next line to it
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_erase_char (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b), *l_n;

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	if ( a_b->cur_col < l_l->len )
		s_line_del(l_l, a_b->cur_col, s_utf8_len(l_l->text + a_b->cur_col, l_l->len - a_b->cur_col));
	else if ( a_b->cur_line + 1 < a_b->nlines )		/* At end of line: join with the next	*/
		{
		l_n = a_b->lines[a_b->cur_line + 1];
		a_b->cur_col = l_l->len;
		s_line_ins(l_l, l_l->len, l_n->text, l_n->len);
		s_buf_del_line(a_b, a_b->cur_line + 1);
		}
	else	return	STS$K_SUCCESS;

	s_buf_touch(a_b);
	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Is the byte part of a word? Letters, digits, underscore and every non-ASCII byte are
 *
 *  INPUTS:
 *	a_c		the byte
 *
 *  RETURNS: non-zero if it is
 */
static int	s_is_word (
	int	a_c
	)
{
	return	isalnum(a_c) || a_c == '_' || (unsigned char) a_c >= 128;
}

/*
 *  DESCRIPTION: Erase from the cursor to the end of the word (or a single non-word character) and the spaces
 *  after it, saving the text for RESTORE. At the end of a line it acts as ERASE CHARACTER
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_restorebuf	the erased text
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_erase_word (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);
unsigned l_from = $MIN(a_b->cur_col, l_l->len), l_to = l_from;

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	if ( l_from == l_l->len )
		return	s_ed_erase_char(a_b);

	if ( s_is_word(l_l->text[l_to]) )
		while ( l_to < l_l->len && s_is_word(l_l->text[l_to]) ) l_to++;
	else	l_to++;

	while ( l_to < l_l->len && l_l->text[l_to] == ' ' ) l_to++;

	s_ed_save_restore(l_l->text + l_from, l_to - l_from);
	s_line_del(l_l, l_from, l_to - l_from);
	a_b->cur_col = l_from;
	s_buf_touch(a_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Erase from the start of the line to the cursor, saving the text for RESTORE
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		modified
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_restorebuf	the erased text
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_ed_erase_sol (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);
unsigned l_col = $MIN(a_b->cur_col, l_l->len);

	if ( TPU$FAILED(s_buf_writable(a_b)) )
		return	STS$K_ERROR;

	if ( !l_col )
		return	STS$K_SUCCESS;

	s_ed_save_restore(l_l->text, l_col);
	s_line_del(l_l, 0, l_col);
	a_b->cur_col = 0;
	s_buf_touch(a_b);

	return	STS$K_SUCCESS;
}

/*
 * =====================================================================================
 *	Movement
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Pull the cursor column inside the current line and onto a character boundary, after a
 *  vertical move
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b->cur_col	adjusted
 *
 *  RETURNS: nothing
 */
static void	s_mv_clamp (
	TPU$BUFFER	*a_b
	)
{
	a_b->cur_col = s_utf8_align(CURLINE(a_b), $MIN(a_b->cur_col, CURLINE(a_b)->len));
}

/*
 *  DESCRIPTION: Move the cursor up, stopping at the first line
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_n		lines to move
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_up (
	TPU$BUFFER	*a_b,
	int		a_n
	)
{
	while ( a_n-- > 0 && a_b->cur_line )
		a_b->cur_line--;

	s_mv_clamp(a_b);
	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Move the cursor down, stopping at the last line
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_n		lines to move
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_down (
	TPU$BUFFER	*a_b,
	int		a_n
	)
{
	while ( a_n-- > 0 && a_b->cur_line + 1 < a_b->nlines )
		a_b->cur_line++;

	s_mv_clamp(a_b);
	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Move the cursor one character left, wrapping to the end of the previous line
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_left (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);

	if ( a_b->cur_col )
		a_b->cur_col = s_utf8_align(l_l, $MIN(a_b->cur_col, l_l->len + 1) - 1);
	else if ( a_b->cur_line )
		a_b->cur_line--, a_b->cur_col = CURLINE(a_b)->len;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Move the cursor one character right, wrapping to the start of the next line
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_right (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);

	if ( a_b->cur_col < l_l->len )
		a_b->cur_col += s_utf8_len(l_l->text + a_b->cur_col, l_l->len - a_b->cur_col);
	else if ( a_b->cur_line + 1 < a_b->nlines )
		a_b->cur_line++, a_b->cur_col = 0;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: EVE MOVE BY LINE: to the end of the line going forward, to the start going in reverse;
 *  already there, it steps to the next (previous) line first
 *
 *  INPUTS:
 *	a_b		the buffer, its reverse flag gives the direction
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_by_line (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l = CURLINE(a_b);

	if ( !a_b->reverse )
		{
		if ( a_b->cur_col >= l_l->len && a_b->cur_line + 1 < a_b->nlines )
			a_b->cur_line++;
		a_b->cur_col = CURLINE(a_b)->len;
		}
	else	{
		if ( !a_b->cur_col && a_b->cur_line )
			a_b->cur_line--;
		a_b->cur_col = 0;
		}

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: EDT's LINE: going forward to the start of the next line (the end of the last one), in
 *  reverse to the start of this line, or of the previous one when already there
 *
 *  INPUTS:
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_by_line_start (
	TPU$BUFFER	*a_b
	)
{
	if ( !a_b->reverse )
		{
		if ( a_b->cur_line + 1 < a_b->nlines )
			a_b->cur_line++, a_b->cur_col = 0;
		else	a_b->cur_col = CURLINE(a_b)->len;
		}
	else	{
		if ( !a_b->cur_col && a_b->cur_line )
			a_b->cur_line--;
		a_b->cur_col = 0;
		}

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Move one word in the buffer's direction: over the word (or one run of punctuation) and the
 *  spaces after it going forward, to the start of the previous word going in reverse
 *
 *  INPUTS:
 *	a_b		the buffer, its reverse flag gives the direction
 *
 *  OUTPUTS:
 *	a_b		cursor moved
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_mv_by_word (
	TPU$BUFFER	*a_b
	)
{
TPU$LINE *	l_l;
unsigned l_c;

	if ( !a_b->reverse )
		{
		l_l = CURLINE(a_b);
		l_c = $MIN(a_b->cur_col, l_l->len);

		if ( l_c >= l_l->len )
			return	s_mv_right(a_b);

		if ( s_is_word(l_l->text[l_c]) )
			while ( l_c < l_l->len && s_is_word(l_l->text[l_c]) ) l_c++;
		else	while ( l_c < l_l->len && !s_is_word(l_l->text[l_c]) && l_l->text[l_c] != ' ' ) l_c++;

		while ( l_c < l_l->len && l_l->text[l_c] == ' ' ) l_c++;
		a_b->cur_col = l_c;
		}
	else	{
		s_mv_left(a_b);
		l_l = CURLINE(a_b);
		l_c = $MIN(a_b->cur_col, l_l->len);

		while ( l_c && l_l->text[l_c - 1] == ' ' ) l_c--;
		if ( l_c && s_is_word(l_l->text[l_c - 1]) )
			while ( l_c && s_is_word(l_l->text[l_c - 1]) ) l_c--;
		else	while ( l_c && !s_is_word(l_l->text[l_c - 1]) && l_l->text[l_c - 1] != ' ' ) l_c--;

		a_b->cur_col = l_c;
		}

	return	STS$K_SUCCESS;
}

/*
 * =====================================================================================
 *	Search. EVE rule: a pattern without upper-case letters matches case-insensitively.
 *	Case is compared on code points with towlower(), so it works for any script the
 *	locale knows about (LANG=C.UTF-8 is enough for Cyrillic).
 * =====================================================================================
 */
/*
 *  DESCRIPTION: EVE rule: a pattern without upper-case letters matches case-insensitively. iswupper() follows
 *  the locale
 *
 *  INPUTS:
 *	a_pat		the pattern
 *
 *  RETURNS: non-zero for a case-insensitive pattern
 */
static int	s_pat_ci (
	const ASC	*a_pat
	)
{
int	l_n;

	for ( int l_i = 0; l_i < a_pat->len; l_i += l_n )
		{
		l_n = s_utf8_len(a_pat->sts + l_i, a_pat->len - l_i);
		if ( iswupper(s_utf8_decode(a_pat->sts + l_i, l_n)) )
			return	0;
		}

	return	1;
}

/*
 *  DESCRIPTION: Does the pattern match the line at a byte offset? Case-insensitive comparison goes by code
 *  points through towlower(), so a match may be longer or shorter than the pattern in bytes
 *
 *  INPUTS:
 *	a_l		the line
 *	a_at		byte offset
 *	a_pat		the pattern
 *	a_ci		non-zero for case-insensitive
 *
 *  RETURNS: the matched length in bytes, 0 if there is no match
 */
static int	s_line_match (
	const TPU$LINE	*a_l,
	unsigned	a_at,
	const ASC	*a_pat,
	int		a_ci
	)
{
unsigned l_i = a_at;
int	l_p = 0, l_nl, l_np;

	if ( !a_ci )
		return	(a_at + a_pat->len <= a_l->len && !memcmp(a_l->text + a_at, a_pat->sts, a_pat->len)) ? a_pat->len : 0;

	while ( l_p < a_pat->len )
		{
		if ( l_i >= a_l->len )
			return	0;

		l_nl = s_utf8_len(a_l->text + l_i, a_l->len - l_i);
		l_np = s_utf8_len(a_pat->sts + l_p, a_pat->len - l_p);

		if ( towlower(s_utf8_decode(a_l->text + l_i, l_nl)) != towlower(s_utf8_decode(a_pat->sts + l_p, l_np)) )
			return	0;

		l_i += l_nl;
		l_p += l_np;
		}

	return	l_i - a_at;
}

static int	s_matchlen;					/* Bytes matched by the last s_buf_search()	*/

/*
 *  DESCRIPTION: Search a buffer from a position in the buffer's direction, only at character boundaries
 *
 *  INPUTS:
 *	a_b		the buffer, its reverse flag gives the direction
 *	a_pat		the pattern
 *	a_from		where to start
 *	a_skip		non-zero: do not accept a match at a_from itself
 *
 *  OUTPUTS:
 *	a_found		position of the match
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_matchlen	bytes matched, for REPLACE
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOSEARCH for an empty pattern, TPU$_NOTFOUND
 */
static int	s_buf_search (
	TPU$BUFFER	*a_b,
	const ASC	*a_pat,
	TPU$POS		a_from,
	int		a_skip,
	TPU$POS		*a_found
	)
{
int	l_ci = s_pat_ci(a_pat), l_col, l_n;
TPU$LINE *	l_l;

	if ( !a_pat->len )
		return	s_tpu_msg(TPU$_NOSEARCH);

	if ( !a_b->reverse )
		{
		for ( int l_line = a_from.line; l_line < (int) a_b->nlines; l_line++ )
			{
			l_l = a_b->lines[l_line];
			l_col = (l_line == (int) a_from.line) ? (int) a_from.col + (a_skip ? 1 : 0) : 0;

			for ( ; l_col < (int) l_l->len; l_col++ )
				{
				if ( s_utf8_cont(l_l->text[l_col]) )	/* Only at character boundaries	*/
					continue;

				if ( (l_n = s_line_match(l_l, l_col, a_pat, l_ci)) )
					{
					a_found->line = l_line, a_found->col = l_col;
					s_matchlen = l_n;
					return	STS$K_SUCCESS;
					}
				}
			}
		}
	else	{
		for ( int l_line = a_from.line; l_line >= 0; l_line-- )
			{
			l_l = a_b->lines[l_line];
			l_col = (l_line == (int) a_from.line) ? (int) $MIN(a_from.col, l_l->len) - (a_skip ? 1 : 0) : (int) l_l->len - 1;

			for ( ; l_col >= 0; l_col-- )
				{
				if ( s_utf8_cont(l_l->text[l_col]) )
					continue;

				if ( (l_n = s_line_match(l_l, l_col, a_pat, l_ci)) )
					{
					a_found->line = l_line, a_found->col = l_col;
					s_matchlen = l_n;
					return	STS$K_SUCCESS;
					}
				}
			}
		}

	return	s_tpu_msg(TPU$_NOTFOUND, a_pat->len, a_pat->sts);
}

/*
 * =====================================================================================
 *	Input: three sources, one exit point.
 *	  1. playback queue  - macro expansions and REPEAT; neither journaled nor learned,
 *	     the key that triggered them already was;
 *	  2. recovery stream - keys read from an old journal by RECOVER, journaled and learned
 *	     as if typed;
 *	  3. terminal.
 * =====================================================================================
 */
static int *	s_recover;
static int	s_recover_len, s_recover_pos;
static int	s_learn_mark;					/* Learn length before the current key	*/

/*
 *  DESCRIPTION: Put a key sequence at the head of the playback queue, in front of whatever is already there:
 *  a macro expands where it was invoked
 *
 *  INPUTS:
 *	a_seq		the keys
 *	a_n		their count
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_pb, s_pb_head, s_pb_tail	the queue
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_QUEOVF when the sequence does not fit
 */
static int	s_pb_push_front (
	const int	*a_seq,
	int		a_n
	)
{
int	l_used = s_pb_tail - s_pb_head;

	if ( !l_used )
		s_pb_head = s_pb_tail = 0;

	if ( s_pb_head < a_n )
		{
		if ( a_n + l_used > TPU$K_PBSZ )
			return	s_tpu_msg(TPU$_QUEOVF);

		memmove(&s_pb[a_n], &s_pb[s_pb_head], l_used * sizeof(int));
		s_pb_head = a_n;
		s_pb_tail = a_n + l_used;
		}

	s_pb_head -= a_n;
	memcpy(&s_pb[s_pb_head], a_seq, a_n * sizeof(int));

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Drop everything queued for playback and the rest of a recovery stream: a command failed, the
 *  replay stops
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_pb_head, s_pb_tail, s_recover_pos	reset
 *
 *  RETURNS: nothing
 */
static void	s_pb_flush (
	void
	)
{
	s_pb_head = s_pb_tail = 0;
	s_recover_pos = s_recover_len;
}

/*
 *  DESCRIPTION: Append a key to the journal, one decimal number per line, flushed at once so that a crash
 *  loses nothing
 *
 *  INPUTS:
 *	a_key		the key
 *
 *  IMPLICIT INPUTS:
 *	s_journal	the journal, may be NULL (-nojournal)
 *
 *  RETURNS: nothing
 */
static void	s_journal_put (
	int	a_key
	)
{
	if ( !s_journal )
		return;

	fprintf(s_journal, "%d\n", a_key);
	fflush(s_journal);
}

/*
 *  DESCRIPTION: Create the journal file, truncating an old one
 *
 *  IMPLICIT INPUTS:
 *	s_journal_name	the file name
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_journal	the stream
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_JNLCREATE
 */
static int	s_journal_open (
	void
	)
{
	if ( !(s_journal = fopen(s_journal_name, "w")) )
		return	s_tpu_msg(TPU$_JNLCREATE, s_journal_name, strerror(errno));

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Close the journal and optionally delete it (a clean EXIT has nothing to recover)
 *
 *  INPUTS:
 *	a_delete	non-zero to delete the file
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_journal	closed
 *
 *  RETURNS: nothing
 */
static void	s_journal_close (
	int	a_delete
	)
{
	if ( s_journal )
		fclose(s_journal), s_journal = NULL;

	if ( a_delete && *s_journal_name )
		unlink(s_journal_name);
}

/*
 *  DESCRIPTION: Read an old journal into the recovery stream for -recover
 *
 *  IMPLICIT INPUTS:
 *	s_journal_name	the file name
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_recover, s_recover_len	the keys read
 *
 *  RETURNS: TPU$_RECOVERING or TPU$_JNLOPEN
 */
static int	s_journal_load (
	void
	)
{
FILE *	l_f;
int	l_k, l_alloc = 1024;

	if ( !(l_f = fopen(s_journal_name, "r")) )
		return	s_tpu_msg(TPU$_JNLOPEN, s_journal_name, strerror(errno));

	s_recover = malloc(l_alloc * sizeof(int));

	while ( fscanf(l_f, "%d", &l_k) == 1 )
		{
		if ( s_recover_len == l_alloc )
			s_recover = realloc(s_recover, (l_alloc *= 2) * sizeof(int));
		s_recover[s_recover_len++] = l_k;
		}

	fclose(l_f);

	return	s_tpu_msg(TPU$_RECOVERING, s_recover_len, s_journal_name);
}

/*
 *  DESCRIPTION: The one place every key comes from, in priority order: the playback queue (macros and REPEAT,
 *  neither journaled nor learned - the key that started them was), the recovery stream, the terminal. Keys
 *  from the last two are learned and journaled. A pending resize is handled here, and a hangup ends the
 *  editor keeping the journal
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_pb_head, s_recover_pos	advanced
 *	s_learn, s_learn_len	the key is recorded while learning
 *	the journal gets the key
 *
 *  RETURNS: the key
 */
static int	s_key_get (
	void
	)
{
int	l_k;

	for (;;)
		{
		if ( s_winch )
			{
			s_scr_resize();
			s_scr_render();
			}

		if ( s_hangup )
			{
			s_term_fini();
			$LOG(STS$K_WARN, "input closed, journal %s is kept", s_journal_name);
			exit(2);
			}

		if ( s_pb_head != s_pb_tail )				/* 1. playback			*/
			return	s_pb[s_pb_head++];

		if ( s_recover_pos < s_recover_len )			/* 2. recovery			*/
			l_k = s_recover[s_recover_pos++];
		else	{						/* 3. terminal			*/
			if ( (l_k = s_term_read_key()) == KEY_NONE || l_k == KEY_CPR )
				continue;
			}

		if ( s_learning )
			{
			if ( s_learn_len < TPU$K_LEARNSZ )
				s_learn[s_learn_len++] = l_k;
			else	{
				s_learning = 0;
				s_tpu_msg(TPU$_LEARNOVF);
				}
			}

		s_journal_put(l_k);

		return	l_k;
		}
}

/*
 *  DESCRIPTION: Human-readable name of a key: "GOLD-KP7", "Ctrl-K", "Delete", "F6", a letter, or "#hex" for
 *  anything else
 *
 *  INPUTS:
 *	a_key		the key
 *	a_sz		size of a_buf
 *
 *  OUTPUTS:
 *	a_buf		the name
 *
 *  RETURNS: a_buf
 */
static const char *	s_key_name (
	int	a_key,
	char	*a_buf,
	size_t	a_sz
	)
{
const char *l_pfx = (a_key & TPU$M_GOLD) ? "GOLD-" : "";
int	l_k = a_key & ~TPU$M_GOLD;

	if ( l_k >= TPU$K_KEYBASE && l_k < KEY_MAX )
		snprintf(a_buf, a_sz, "%s%s", l_pfx, s_keynames[l_k - TPU$K_KEYBASE]);
	else if ( l_k == 127 )
		snprintf(a_buf, a_sz, "%sDelete", l_pfx);
	else if ( l_k == 32 )
		snprintf(a_buf, a_sz, "%sSpace", l_pfx);
	else if ( l_k == 13 )
		snprintf(a_buf, a_sz, "%sReturn", l_pfx);
	else if ( l_k == 9 )
		snprintf(a_buf, a_sz, "%sTab", l_pfx);
	else if ( l_k == 27 )
		snprintf(a_buf, a_sz, "%sEscape", l_pfx);
	else if ( l_k < 32 )
		snprintf(a_buf, a_sz, "%sCtrl-%c", l_pfx, l_k + '@');
	else if ( l_k < 256 )
		snprintf(a_buf, a_sz, "%s%c", l_pfx, l_k);
	else	snprintf(a_buf, a_sz, "%s#%x", l_pfx, l_k);

	return	a_buf;
}

/*
 * =====================================================================================
 *	Prompts on the message line. Read through s_key_get(), so they replay correctly.
 * =====================================================================================
 */
#define	TPU$K_HISTSZ	32

static char	s_hist[TPU$K_HISTSZ][256];			/* Recall ring for prompts	*/
static int	s_nhist;

/*
 *  DESCRIPTION: Ask on the message line and read the answer through s_key_get(), so that it replays
 *  correctly. Return or Enter ends the answer, Delete erases, Up/Down and Ctrl-B recall earlier answers,
 *  Ctrl-U clears the line, Ctrl-Z, ESC and Ctrl-C cancel. Non-empty answers go into the recall ring
 *
 *  INPUTS:
 *	a_label		the prompt
 *	a_sz		size of a_out
 *
 *  OUTPUTS:
 *	a_out		the answer, NUL-terminated, empty on Return alone
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_prompt_label, s_prompt_text	set while the prompt is active
 *	s_hist, s_nhist	the recall ring
 *	s_msg		cleared afterwards
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_CANCEL
 */
static int	s_prompt (
	const char	*a_label,
	char		*a_out,
	int		a_sz
	)
{
int	l_len = 0, l_k, l_h = s_nhist;

	a_out[0] = '\0';
	s_prompt_label = a_label;
	s_prompt_text = a_out;

	for (;;)
		{
		if ( !s_utf8_incomplete(a_out, l_len) )
			s_scr_render();
		l_k = s_key_get();

		if ( l_k == 13 || l_k == 10 || l_k == KEY_KPENTER )
			break;

		if ( l_k == 127 || l_k == 8 )
			{
			while ( l_len && s_utf8_cont(a_out[--l_len]) );
			}
		else if ( l_k == KEY_UP || l_k == KEY_DOWN || l_k == 0x02 )	/* Recall, Ctrl-B as in EVE	*/
			{
			if ( !s_nhist )
				continue;
			l_h = (l_k == KEY_DOWN) ? $MIN(l_h + 1, s_nhist) : $MAX(l_h - 1, 0);
			snprintf(a_out, a_sz, "%s", l_h < s_nhist ? s_hist[l_h] : "");
			l_len = strlen(a_out);
			}
		else if ( l_k == 0x15 )				/* Ctrl-U			*/
			l_len = 0;
		else if ( l_k == 0x1a || l_k == 0x1b || l_k == 0x03 )
			{
			s_prompt_label = NULL;
			return	s_tpu_msg(TPU$_CANCEL);
			}
		else if ( l_k >= 32 && l_k < 256 && l_len < a_sz - 1 )
			a_out[l_len++] = l_k;

		a_out[l_len] = '\0';
		}

	if ( *a_out && (!s_nhist || strcmp(s_hist[s_nhist - 1], a_out)) )
		{
		if ( s_nhist == TPU$K_HISTSZ )
			memmove(s_hist[0], s_hist[1], sizeof(s_hist) - sizeof(s_hist[0])), s_nhist--;
		snprintf(s_hist[s_nhist++], sizeof(s_hist[0]), "%s", a_out);
		}

	s_prompt_label = NULL;
	s_msg[0] = '\0';

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Take the argument typed after the command, or prompt for one when there is none
 *
 *  INPUTS:
 *	a_arg		the argument, may be NULL or empty
 *	a_label		the prompt
 *	a_sz		size of a_out
 *
 *  OUTPUTS:
 *	a_out		the value
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_CANCEL from the prompt
 */
static int	s_arg_or_prompt (
	char		*a_arg,
	const char	*a_label,
	char		*a_out,
	int		a_sz
	)
{
	if ( a_arg && *a_arg )
		{
		snprintf(a_out, a_sz, "%s", a_arg);
		return	STS$K_SUCCESS;
		}

	return	s_prompt(a_label, a_out, a_sz);
}

/*
 *  DESCRIPTION: Ask a yes/no question until the answer starts with Y or N
 *
 *  INPUTS:
 *	a_label		the question
 *
 *  RETURNS: 1 for yes, 0 for no, -1 when the prompt was cancelled
 */
static int	s_yesno (
	const char	*a_label
	)
{
char	l_ans[16];

	for (;;)
		{
		if ( TPU$FAILED(s_prompt(a_label, l_ans, sizeof(l_ans))) )
			return	-1;

		if ( tolower((unsigned char) l_ans[0]) == 'y' )
			return	1;

		if ( tolower((unsigned char) l_ans[0]) == 'n' )
			return	0;
		}
}

/* Read one key for "press the key ..." questions, expanding a GOLD prefix */
static int	s_key_ask(const char *a_label);

/*
 * =====================================================================================
 *	Command table and key bindings
 * =====================================================================================
 */
static const TPU$CMD s_cmds[];
static int	s_ncmds;

/*
 *  DESCRIPTION: Look a key up in the definition table
 *
 *  INPUTS:
 *	a_key		the key, with TPU$M_GOLD for a GOLD sequence
 *
 *  IMPLICIT INPUTS:
 *	s_keydefs, s_nkeydefs	the table
 *
 *  RETURNS: the definition, or NULL
 */
static TPU$KEYDEF *	s_keydef_find (
	int	a_key
	)
{

	for ( int l_i = 0; l_i < s_nkeydefs; l_i++ )
		if ( s_keydefs[l_i].key == a_key )
			return	&s_keydefs[l_i];

	return	NULL;
}

/*
 *  DESCRIPTION: Define or redefine a key: a command or a learned sequence (which is copied)
 *
 *  INPUTS:
 *	a_key		the key
 *	a_type		KD$K_CMD or KD$K_MACRO
 *	a_cmd		the command for KD$K_CMD
 *	a_seq, a_seqlen	the keys for KD$K_MACRO
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_keydefs, s_nkeydefs	the table
 *
 *  RETURNS: the definition, or NULL when the table is full
 */
static TPU$KEYDEF *	s_keydef_set (
	int		a_key,
	int		a_type,
	const TPU$CMD	*a_cmd,
	const int	*a_seq,
	int		a_seqlen
	)
{
TPU$KEYDEF *l_kd = s_keydef_find(a_key);

	if ( !l_kd )
		{
		if ( s_nkeydefs >= TPU$K_MAXKEYDEF )
			return	NULL;
		l_kd = &s_keydefs[s_nkeydefs++];
		}
	else if ( l_kd->seq )
		free(l_kd->seq), l_kd->seq = NULL;

	l_kd->key = a_key;
	l_kd->type = a_type;
	l_kd->cmd = a_cmd;
	l_kd->seqlen = a_seqlen;

	if ( a_type == KD$K_MACRO )
		{
		l_kd->seq = malloc(a_seqlen * sizeof(int));
		memcpy(l_kd->seq, a_seq, a_seqlen * sizeof(int));
		}

	return	l_kd;
}

/*
 *  DESCRIPTION: Match a command line against the table: input words must be case-insensitive prefixes of the
 *  command's words, in order; the first word is mandatory, further words are taken while they match and
 *  whatever is left is the argument ("GET foo.c" is GET FILE foo.c, "TWO" is TWO WINDOWS). Exact words beat
 *  prefixes, more words beat fewer, a tie is ambiguous
 *
 *  INPUTS:
 *	a_line		the command line, modified: the argument is trimmed in place
 *
 *  OUTPUTS:
 *	a_cmd		the command
 *	a_arg		the argument, pointing into a_line, empty when there is none
 *
 *  IMPLICIT INPUTS:
 *	s_cmds, s_ncmds	the command table
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_UNKCMD or TPU$_AMBCMD
 */
static int	s_cmd_lookup (
	char		*a_line,
	const TPU$CMD	**a_cmd,
	char		**a_arg
	)
{
const TPU$CMD *l_best = NULL;
int	l_nbest = 0, l_bestscore = 0, l_score, l_nw, l_exact;
char *	l_p, *l_w, *l_in, *l_bestarg = NULL;
char	l_name[64];

	while ( *a_line == ' ' ) a_line++;

	for ( int l_i = 0; l_i < s_ncmds; l_i++ )
		{
		snprintf(l_name, sizeof(l_name), "%s", s_cmds[l_i].name);
		l_in = a_line;
		l_nw = 0;
		l_exact = 1;

		for ( l_w = strtok_r(l_name, " ", &l_p); l_w; l_w = strtok_r(NULL, " ", &l_p) )
			{
			int	l_wl;

			while ( *l_in == ' ' ) l_in++;
			for ( l_wl = 0; l_in[l_wl] && l_in[l_wl] != ' '; l_wl++ );

			if ( !l_wl || l_wl > (int) strlen(l_w) || strncasecmp(l_in, l_w, l_wl) )
				{
				if ( !l_nw )				/* First word is mandatory	*/
					l_nw = -1;
				break;
				}

			if ( l_wl != (int) strlen(l_w) )
				l_exact = 0;

			l_in += l_wl;
			l_nw++;
			}

		if ( l_nw < 0 )
			continue;

		l_score = l_nw * 4 + l_exact * 2 + (l_w ? 0 : 1);	/* Whole name matched: +1	*/

		if ( l_score > l_bestscore )
			l_best = &s_cmds[l_i], l_bestscore = l_score, l_nbest = 1, l_bestarg = l_in;
		else if ( l_score == l_bestscore )
			l_nbest++;
		}

	if ( !l_best )
		return	s_tpu_msg(TPU$_UNKCMD, a_line);

	if ( l_nbest > 1 )
		return	s_tpu_msg(TPU$_AMBCMD, a_line);

	while ( *l_bestarg == ' ' ) l_bestarg++;
	for ( l_p = l_bestarg + strlen(l_bestarg); l_p > l_bestarg && l_p[-1] == ' '; l_p-- );
	*l_p = '\0';

	*a_cmd = l_best;
	*a_arg = l_bestarg;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Look a command up by its exact name, case-insensitively
 *
 *  INPUTS:
 *	a_name		the name
 *
 *  IMPLICIT INPUTS:
 *	s_cmds, s_ncmds	the command table
 *
 *  RETURNS: the command, or NULL
 */
static const TPU$CMD *	s_cmd_by_name (
	const char	*a_name
	)
{

	for ( int l_i = 0; l_i < s_ncmds; l_i++ )
		if ( !strcasecmp(s_cmds[l_i].name, a_name) )
			return	&s_cmds[l_i];

	return	NULL;
}

/*
 * =====================================================================================
 *	Key files: the key table as text, "keys = command" per line, loaded at start
 *	(~/.tpu.keys, ./tpu.keys, -keys file or TPU_KEYS) and by LOAD KEYS, written by
 *	SAVE KEYS. Key names are those of SHOW KEYS, several keys go through a comma,
 *	NONE unbinds, a learned sequence is KEYS "text{Name}text": bytes as themselves,
 *	special keys in braces, \" \\ \{ escaped. "!" starts a comment.
 * =====================================================================================
 */
#define	TPU$K_KEYFILE		"tpu.keys"			/* In the current directory		*/
#define	TPU$K_KEYFILE_HOME	".tpu.keys"			/* In $HOME				*/

/*
 *  DESCRIPTION: The inverse of s_key_name(): a key from its name. "GOLD-" prefix, a name from tpukeys.h
 *  (F4, KP7, Insert Here, Begin; PF1-PF4 for F1-F4 too), Delete, Space, Return, Tab, Escape or Esc, Ctrl-X or ^X, #hex, or a
 *  single character. Case does not matter for the names, it does for a single letter
 *
 *  INPUTS:
 *	a_name		the name, not NUL-terminated; a_len - its length
 *
 *  OUTPUTS:
 *	a_key		the key
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a name that is not a key (no message)
 */
static int	s_key_parse (
	const char	*a_name,
	int		a_len,
	int		*a_key
	)
{
int	l_gold = 0, l_c;
char	l_buf[32];

	while ( a_len && (*a_name == ' ' || *a_name == '\t') )
		a_name++, a_len--;
	while ( a_len && (a_name[a_len - 1] == ' ' || a_name[a_len - 1] == '\t') )
		a_len--;

	if ( a_len > 5 && !strncasecmp(a_name, "GOLD-", 5) )
		l_gold = TPU$M_GOLD, a_name += 5, a_len -= 5;

	if ( !a_len || a_len >= (int) sizeof(l_buf) )
		return	STS$K_ERROR;

	memcpy(l_buf, a_name, a_len);
	l_buf[a_len] = '\0';

	for ( int l_i = 0; l_i < (int) $ARRSZ(s_keynames); l_i++ )
		if ( !strcasecmp(l_buf, s_keynames[l_i]) )
			return	*a_key = l_gold | (TPU$K_KEYBASE + l_i), STS$K_SUCCESS;

	if ( a_len == 3 && toupper((unsigned char) l_buf[0]) == 'P' && toupper((unsigned char) l_buf[1]) == 'F'
		&& l_buf[2] >= '1' && l_buf[2] <= '4' )			/* VT names of F1-F4, as in older key files	*/
		return	*a_key = l_gold | (KEY_PF1 + l_buf[2] - '1'), STS$K_SUCCESS;

	if ( !strcasecmp(l_buf, "Delete") )
		l_c = 127;
	else if ( !strcasecmp(l_buf, "Space") )
		l_c = 32;
	else if ( !strcasecmp(l_buf, "Return") )
		l_c = 13;
	else if ( !strcasecmp(l_buf, "Tab") )
		l_c = 9;
	else if ( !strcasecmp(l_buf, "Escape") || !strcasecmp(l_buf, "Esc") )
		l_c = 27;
	else if ( (a_len == 6 && !strncasecmp(l_buf, "Ctrl-", 5)) || (a_len == 2 && l_buf[0] == '^') )
		{
		l_c = toupper((unsigned char) l_buf[a_len - 1]);

		if ( l_c < '@' || l_c > '_' )
			return	STS$K_ERROR;

		l_c -= '@';
		}
	else if ( l_buf[0] == '#' && a_len > 1 )
		{
		char	*l_end;

		l_c = strtol(l_buf + 1, &l_end, 16);

		if ( *l_end || l_c < 0 )
			return	STS$K_ERROR;
		}
	else if ( a_len == 1 )
		l_c = (unsigned char) l_buf[0];
	else	return	STS$K_ERROR;

	*a_key = l_gold | l_c;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: A learned sequence as the text of a KEYS "..." clause: printable bytes as themselves, UTF-8
 *  bytes raw (the character comes out whole), " \ { escaped with a backslash, everything else as {Name}
 *
 *  INPUTS:
 *	a_seq		the keys; a_n - their count; a_sz - size of a_out
 *
 *  OUTPUTS:
 *	a_out		the text without the quotes, NUL-terminated, cut when a_out is too small
 *
 *  RETURNS: nothing
 */
static void	s_macro_text (
	const int	*a_seq,
	int		a_n,
	char		*a_out,
	int		a_sz
	)
{
int	l_o = 0, l_k;
char	l_name[32];

	for ( int l_i = 0; l_i < a_n && l_o + 40 < a_sz; l_i++ )
		{
		l_k = a_seq[l_i];

		if ( l_k == '"' || l_k == '\\' || l_k == '{' )
			a_out[l_o++] = '\\', a_out[l_o++] = l_k;
		else if ( (l_k >= 32 && l_k < 127) || (l_k >= 128 && l_k < 256) )
			a_out[l_o++] = l_k;
		else	l_o += snprintf(a_out + l_o, a_sz - l_o, "{%s}", s_key_name(l_k, l_name, sizeof(l_name)));
		}

	a_out[l_o] = '\0';
}

/*
 *  DESCRIPTION: Parse the body of a KEYS "..." clause into keys, see s_macro_text() for the notation
 *
 *  INPUTS:
 *	a_s		text after the opening quote; a_max - room in a_seq
 *
 *  OUTPUTS:
 *	a_seq		the keys; a_n - their count
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a missing closing quote or brace, an unknown {Name}, or too
 *  many keys (no message)
 */
static int	s_macro_parse (
	const char	*a_s,
	int		*a_seq,
	int		a_max,
	int		*a_n
	)
{
const char *l_p = a_s, *l_q;
int	l_n = 0, l_k;

	while ( *l_p && *l_p != '"' )
		{
		if ( l_n >= a_max )
			return	STS$K_ERROR;

		if ( *l_p == '\\' && l_p[1] )
			a_seq[l_n++] = (unsigned char) l_p[1], l_p += 2;
		else if ( *l_p == '{' )
			{
			if ( !(l_q = strchr(l_p, '}')) || TPU$FAILED(s_key_parse(l_p + 1, l_q - l_p - 1, &l_k)) )
				return	STS$K_ERROR;

			a_seq[l_n++] = l_k;
			l_p = l_q + 1;
			}
		else	a_seq[l_n++] = (unsigned char) *l_p++;
		}

	if ( *l_p != '"' )
		return	STS$K_ERROR;

	*a_n = l_n;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Remove a key from the definition table (NONE in a key file)
 *
 *  INPUTS:
 *	a_key		the key; not defined - nothing happens
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_keydefs, s_nkeydefs	the table; the last entry moves into the hole
 *
 *  RETURNS: nothing
 */
static void	s_keydef_del (
	int	a_key
	)
{
TPU$KEYDEF *l_kd = s_keydef_find(a_key);

	if ( !l_kd )
		return;

	if ( l_kd->seq )
		free(l_kd->seq);

	*l_kd = s_keydefs[--s_nkeydefs];
	memset(&s_keydefs[s_nkeydefs], 0, sizeof(s_keydefs[0]));
}

/*
 *  DESCRIPTION: Next key name from the left side of a key file line. Names are separated by commas, but a
 *  comma is part of the name when what precedes it is nothing, "KP", "GOLD-" or "GOLD-KP": the keys
 *  ",", "KP,", "GOLD-," and "GOLD-KP,"
 *
 *  INPUTS:
 *	a_p		cursor into the line, advanced past the name and its comma; a_end - end of the key part
 *
 *  OUTPUTS:
 *	a_tok, a_len	the name, not NUL-terminated
 *
 *  RETURNS: 1 when a name was taken, 0 at the end
 */
static int	s_keys_next (
	const char	**a_p,
	const char	*a_end,
	const char	**a_tok,
	int		*a_len
	)
{
const char *l_p = *a_p, *l_start;
int	l_l;

	while ( l_p < a_end && (*l_p == ' ' || *l_p == '\t') )
		l_p++;

	if ( l_p >= a_end )
		return	0;

	for ( l_start = l_p; l_p < a_end; l_p++ )
		if ( *l_p == ',' )
			{
			l_l = l_p - l_start;

			if ( !(!l_l || (l_l == 2 && !strncasecmp(l_start, "KP", 2)) || (l_l == 5 && !strncasecmp(l_start, "GOLD-", 5))
				|| (l_l == 7 && !strncasecmp(l_start, "GOLD-KP", 7))) )
				break;
			}

	*a_tok = l_start;
	*a_len = l_p - l_start;
	*a_p = (l_p < a_end) ? l_p + 1 : l_p;

	return	1;
}

/*
 *  DESCRIPTION: Load a key file, every line rebinding what it names; bad lines are reported one by one
 *  with their number and skipped
 *
 *  INPUTS:
 *	a_file		the file; a_quiet - non-zero for a default location: a missing file is fine and success
 *			is not reported, only errors are
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_keydefs, s_nkeydefs	the table
 *
 *  RETURNS: TPU$_KEYSLOADED, TPU$_KEYSERRORS when some lines were bad, TPU$_OPENIN, or STS$K_SUCCESS for a
 *  quiet load
 */
static int	s_keys_load (
	const char	*a_file,
	int		a_quiet
	)
{
FILE *	l_f;
char *	l_line = NULL, *l_p, *l_eq, *l_cmdtext, *l_q;
const char *l_kp, *l_tok;
const TPU$CMD *l_cmd;
size_t	l_cap = 0;
ssize_t	l_n;
int	l_lno = 0, l_nbind = 0, l_nerr = 0, l_type, l_len, l_key, l_seqlen = 0;
int	l_seq[TPU$K_LEARNSZ];

	if ( !(l_f = fopen(a_file, "r")) )
		{
		if ( a_quiet && errno == ENOENT )
			return	STS$K_SUCCESS;

		return	s_tpu_msg(TPU$_OPENIN, a_file, strerror(errno));
		}

	while ( (l_n = getline(&l_line, &l_cap, l_f)) > 0 )
		{
		l_lno++;

		while ( l_n && (l_line[l_n - 1] == '\n' || l_line[l_n - 1] == '\r') )
			l_line[--l_n] = '\0';

		for ( l_p = l_line; *l_p == ' ' || *l_p == '\t'; l_p++ );

		if ( !*l_p || *l_p == '!' )
			continue;

		/* The separator: the first "=" that has a blank before it, so that "=" itself can be a key */
		for ( l_eq = NULL, l_q = l_p + 1; *l_q; l_q++ )
			if ( *l_q == '=' && (l_q[-1] == ' ' || l_q[-1] == '\t') )
				{
				l_eq = l_q;
				break;
				}

		if ( !l_eq )
			{
			l_nerr++;
			s_tpu_msg(TPU$_BADKEYLINE, a_file, l_lno);
			continue;
			}

		for ( l_cmdtext = l_eq + 1; *l_cmdtext == ' ' || *l_cmdtext == '\t'; l_cmdtext++ );

		/* The right side: KEYS "...", NONE, or a command name; a comment may follow */
		if ( !strncasecmp(l_cmdtext, "KEYS", 4) && (l_cmdtext[4] == ' ' || l_cmdtext[4] == '\t') )
			{
			for ( l_q = l_cmdtext + 4; *l_q == ' ' || *l_q == '\t'; l_q++ );

			if ( *l_q != '"' || TPU$FAILED(s_macro_parse(l_q + 1, l_seq, TPU$K_LEARNSZ, &l_seqlen)) || !l_seqlen )
				{
				l_nerr++;
				s_tpu_msg(TPU$_BADMACRO, a_file, l_lno);
				continue;
				}

			l_type = KD$K_MACRO;
			l_cmd = NULL;
			}
		else	{
			if ( (l_q = strstr(l_cmdtext, " !")) || (l_q = strstr(l_cmdtext, "\t!")) )
				*l_q = '\0';

			for ( l_q = l_cmdtext + strlen(l_cmdtext); l_q > l_cmdtext && (l_q[-1] == ' ' || l_q[-1] == '\t'); l_q-- );
			*l_q = '\0';

			if ( !strcasecmp(l_cmdtext, "NONE") )
				l_type = 0, l_cmd = NULL;
			else if ( (l_cmd = s_cmd_by_name(l_cmdtext)) )
				l_type = KD$K_CMD;
			else	{
				l_nerr++;
				s_tpu_msg(TPU$_BADKEYCMD, a_file, l_lno, l_cmdtext);
				continue;
				}
			}

		/* The left side: the keys */
		for ( l_kp = l_p; s_keys_next(&l_kp, l_eq, &l_tok, &l_len); )
			{
			if ( TPU$FAILED(s_key_parse(l_tok, l_len, &l_key)) )
				{
				l_nerr++;
				s_tpu_msg(TPU$_BADKEY, a_file, l_lno, l_len, l_tok);
				continue;
				}

			if ( !l_type )
				s_keydef_del(l_key);
			else if ( !s_keydef_set(l_key, l_type, l_cmd, l_seq, l_seqlen) )
				{
				l_nerr++;
				s_tpu_msg(TPU$_KEYTBLFULL);
				continue;
				}

			l_nbind++;
			}
		}

	free(l_line);
	fclose(l_f);

	if ( l_nerr )
		return	s_tpu_msg(TPU$_KEYSERRORS, l_nbind, a_file, l_nerr);

	if ( !a_quiet )
		return	s_tpu_msg(TPU$_KEYSLOADED, l_nbind, a_file);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Write the live key table as a key file: the keys of one command on one line, in table order,
 *  a learned sequence as KEYS "..."
 *
 *  INPUTS:
 *	a_file		the file, replaced
 *
 *  IMPLICIT INPUTS:
 *	s_keydefs, s_nkeydefs	the table
 *
 *  RETURNS: TPU$_KEYSSAVED, TPU$_OPENOUT or TPU$_WRITEERR
 */
static int	s_keys_save (
	const char	*a_file
	)
{
FILE *	l_f;
char	l_keys[1024], l_name[32], l_text[TPU$K_LEARNSZ * 8 + 1];
char	l_done[TPU$K_MAXKEYDEF] = {0};
int	l_lines = 0, l_o;

	if ( !(l_f = fopen(a_file, "w")) )
		return	s_tpu_msg(TPU$_OPENOUT, a_file, strerror(errno));

	fprintf(l_f, "! %s key definitions, written by SAVE KEYS. One line: keys = command.\n", __MODULE__);
	fprintf(l_f, "! Key names as in SHOW KEYS, several keys through a comma, NONE unbinds,\n");
	fprintf(l_f, "! KEYS \"text{Name}text\" is a learned sequence (\\\" \\\\ \\{ escaped).\n");

	for ( int l_i = 0; l_i < s_nkeydefs; l_i++ )
		{
		if ( l_done[l_i] )
			continue;

		if ( s_keydefs[l_i].type == KD$K_MACRO )
			{
			s_macro_text(s_keydefs[l_i].seq, s_keydefs[l_i].seqlen, l_text, sizeof(l_text));
			fprintf(l_f, "%-28s = KEYS \"%s\"\n", s_key_name(s_keydefs[l_i].key, l_name, sizeof(l_name)), l_text);
			l_lines++;
			continue;
			}

		l_o = 0;

		for ( int l_k = l_i; l_k < s_nkeydefs; l_k++ )	/* Every key of this command	*/
			{
			if ( l_done[l_k] || s_keydefs[l_k].type != KD$K_CMD || s_keydefs[l_k].cmd != s_keydefs[l_i].cmd )
				continue;

			if ( l_o + 40 >= (int) sizeof(l_keys) )
				break;

			l_o += snprintf(l_keys + l_o, sizeof(l_keys) - l_o, "%s%s", l_o ? ", " : "", s_key_name(s_keydefs[l_k].key, l_name, sizeof(l_name)));
			l_done[l_k] = 1;
			}

		fprintf(l_f, "%-28s = %s\n", l_keys, s_keydefs[l_i].cmd->name);
		l_lines++;
		}

	if ( fclose(l_f) )
		return	s_tpu_msg(TPU$_WRITEERR, a_file, strerror(errno));

	return	s_tpu_msg(TPU$_KEYSSAVED, l_lines, a_file);
}

/*
 * =====================================================================================
 *	Dispatcher
 * =====================================================================================
 */
/*
 *  DESCRIPTION: Act on a key: run its command, queue its learned sequence, or insert it as text. Undefined
 *  GOLD sequences and control keys are reported
 *
 *  INPUTS:
 *	a_key		the key
 *
 *  RETURNS: the status of what was done; TPU$_KEYUNDEF for an undefined key
 */
static int	s_dispatch (
	int	a_key
	)
{
TPU$KEYDEF *l_kd = s_keydef_find(a_key);
char	l_name[32];

	if ( l_kd )
		{
		if ( l_kd->type == KD$K_MACRO )
			return	s_pb_push_front(l_kd->seq, l_kd->seqlen);

		return	l_kd->cmd->fn("");
		}

	if ( a_key & TPU$M_GOLD )
		return	s_tpu_msg(TPU$_KEYUNDEF, s_key_name(a_key, l_name, sizeof(l_name)));

	if ( a_key >= 32 && a_key != 127 && a_key < 256 )
		return	s_ed_insert_char(CURBUF, a_key);

	return	s_tpu_msg(TPU$_KEYUNDEF, s_key_name(a_key, l_name, sizeof(l_name)));
}

/*
 *  DESCRIPTION: Is the key currently bound to GOLD? DEFINE KEY may have moved the prefix
 *
 *  INPUTS:
 *	a_key		the key
 *
 *  RETURNS: non-zero if it is
 */
static int	s_is_gold (
	int	a_key
	)
{
TPU$KEYDEF *l_kd = s_keydef_find(a_key);

	return	l_kd && l_kd->type == KD$K_CMD && !strcmp(l_kd->cmd->name, "GOLD");
}

/*
 *  DESCRIPTION: Ask for a single key ("press the key ...") on the message line, expanding a GOLD prefix into
 *  the GOLD form of the next key
 *
 *  INPUTS:
 *	a_label		the prompt
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_prompt_label, s_prompt_text	set while asking
 *	s_msg		cleared afterwards
 *
 *  RETURNS: the key
 */
static int	s_key_ask (
	const char	*a_label
	)
{
int	l_k;

	s_prompt_label = a_label;
	s_prompt_text = "";
	s_scr_render();
	l_k = s_key_get();

	if ( s_is_gold(l_k) )
		l_k = s_key_get() | TPU$M_GOLD;

	s_prompt_label = NULL;
	s_msg[0] = '\0';

	return	l_k;
}

/*
 *  DESCRIPTION: Show a system buffer (HELP, $BUFFERS, $KEYS) in the current window. Without a prompt: until
 *  a key that is not a navigation command is pressed, that key is consumed. With a prompt (VMS HELP's
 *  "Topic? "): navigation keys scroll, printable keys type the answer, Delete erases, Ctrl-U clears, Return
 *  ends, Ctrl-Z and Ctrl-C cancel. The window's own buffer and scroll position are restored afterwards
 *
 *  INPUTS:
 *	a_b		the buffer to show
 *	a_label		the prompt, NULL for none; a_sz - size of a_out
 *
 *  OUTPUTS:
 *	a_out		the answer, NUL-terminated, when a_label is given
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_win[s_curwin]	temporarily switched
 *	s_msg, s_prompt_label	"press Return" or the prompt, then cleared
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_CANCEL when the prompt was cancelled
 */
static int	s_show_sysbuf_ask (
	TPU$BUFFER	*a_b,
	const char	*a_label,
	char		*a_out,
	int		a_sz
	)
{
TPU$WINDOW *l_w = CURWIN;
TPU$BUFFER *l_save = l_w->buf;
unsigned l_top = l_w->top, l_left = l_w->left;
TPU$KEYDEF *l_kd;
int	l_k, l_len = 0, l_sts = STS$K_SUCCESS;

	l_w->buf = a_b;
	l_w->top = a_b->cur_line;
	l_w->left = 0;

	if ( a_label )
		a_out[0] = '\0';
	else	s_tpu_msg(TPU$_PRESSRET);

	for (;;)
		{
		if ( a_label )
			{
			s_prompt_label = a_label;		/* Every pass: GOLD digits borrow the label	*/
			s_prompt_text = a_out;
			}

		s_scr_render();
		l_k = s_key_get();
		l_kd = s_keydef_find(l_k);

		if ( !a_label )
			{
			if ( l_kd && l_kd->type == KD$K_CMD && (l_kd->cmd->flags & CMD$M_NAV) )
				s_dispatch(l_k);
			else	break;

			continue;
			}

		if ( l_k == 13 || l_k == 10 || l_k == KEY_KPENTER )
			break;

		if ( l_k == 0x1a || l_k == 0x03 )
			{
			l_sts = TPU$_CANCEL;
			break;
			}

		if ( l_k == 127 || l_k == 8 )
			{
			while ( l_len && s_utf8_cont(a_out[--l_len]) );
			a_out[l_len] = '\0';
			}
		else if ( l_k == 0x15 )
			a_out[l_len = 0] = '\0';
		else if ( l_kd && l_kd->type == KD$K_CMD && (l_kd->cmd->flags & CMD$M_NAV) && !(l_k >= 32 && l_k < 256) )
			s_dispatch(l_k);			/* Scrolling; a plain printable key is text	*/
		else if ( l_k >= 32 && l_k < 256 && l_len < a_sz - 1 )
			a_out[l_len++] = l_k, a_out[l_len] = '\0';
		}

	l_w->buf = l_save;
	l_w->top = l_top;
	l_w->left = l_left;
	s_prompt_label = NULL;
	s_msg[0] = '\0';

	return	(l_sts == TPU$_CANCEL) ? s_tpu_msg(TPU$_CANCEL) : STS$K_SUCCESS;
}

/* Show a system buffer until a non-navigation key. RETURNS STS$K_SUCCESS */
static int	s_show_sysbuf (
	TPU$BUFFER	*a_b
	)
{
	return	s_show_sysbuf_ask(a_b, NULL, NULL, 0);
}

/*
 *  DESCRIPTION: Append strings to a buffer laid out in columns across the screen width, row by row, as VMS
 *  HELP lists its topics: the column width is the longest string plus two, as many columns as fit
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_items		the strings; a_n - their count
 *
 *  IMPLICIT INPUTS:
 *	s_cols		the screen width
 *
 *  RETURNS: nothing
 */
static void	s_buf_columns (
	TPU$BUFFER	*a_b,
	const char	**a_items,
	int		a_n
	)
{
int	l_maxlen = 1, l_colw, l_ncols, l_o;
char	l_row[s_cols * 4 + 64];

	for ( int l_i = 0; l_i < a_n; l_i++ )
		l_maxlen = $MAX(l_maxlen, (int) strlen(a_items[l_i]));

	l_colw = l_maxlen + 2;
	l_ncols = $MAX(1, (s_cols - 2) / l_colw);

	for ( int l_i = 0; l_i < a_n; l_i += l_ncols )
		{
		l_o = snprintf(l_row, sizeof(l_row), "  ");

		for ( int l_c = 0; l_c < l_ncols && l_i + l_c < a_n && l_o + l_colw < (int) sizeof(l_row); l_c++ )
			l_o += snprintf(l_row + l_o, sizeof(l_row) - l_o, "%-*s", l_colw, a_items[l_i + l_c]);

		while ( l_o && l_row[l_o - 1] == ' ' )
			l_o--;

		s_buf_append_line(a_b, l_row, l_o);
		}
}

/*
 *  DESCRIPTION: Replace a buffer's content with a NULL-terminated array of C strings (the embedded help text)
 *
 *  INPUTS:
 *	a_b		the buffer
 *	a_lines		the strings
 *
 *  OUTPUTS:
 *	a_b		the new content, at least one line
 *
 *  RETURNS: nothing
 */
static void	s_buf_set_lines (
	TPU$BUFFER	*a_b,
	const char	**a_lines
	)
{
	s_buf_clear(a_b);
	s_line_free(a_b->lines[0]);
	a_b->nlines = 0;

	for ( ; *a_lines; a_lines++ )
		s_buf_ins_line(a_b, a_b->nlines, s_line_new(*a_lines, strlen(*a_lines)));

	if ( !a_b->nlines )
		s_buf_ins_line(a_b, 0, s_line_new("", 0));
}

/*
 *  DESCRIPTION: Show a buffer in a window, remembering the previous one for "BUFFER" without a name, and
 *  scroll to the top left
 *
 *  INPUTS:
 *	a_w		the window
 *	a_b		the buffer
 *
 *  OUTPUTS:
 *	a_w		buf, prev, top, left
 *
 *  RETURNS: nothing
 */
static void	s_win_set_buf (
	TPU$WINDOW	*a_w,
	TPU$BUFFER	*a_b
	)
{
	if ( a_w->buf != a_b )
		a_w->prev = a_w->buf;

	a_w->buf = a_b;
	a_w->top = a_w->left = 0;
}

/*
 * =====================================================================================
 *	Commands
 * =====================================================================================
 */
/*
 *  DESCRIPTION: GOLD: read the next key and dispatch its GOLD form. GOLD followed by digits is the EDT repeat
 *  count: the key after the digits (GOLD-expanded if need be) is queued that many times
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_prompt_label	shows the count while digits are typed
 *	the playback queue
 *
 *  RETURNS: the status of the dispatched command, or of queueing the repeat
 */
static int	s_cmd_gold (
	char	*a_arg
	)
{
int	l_k, l_n = 0;
char	l_lbl[48];

	l_k = s_key_get();

	if ( l_k < 256 && isdigit(l_k) )				/* GOLD digits key: EDT repeat count	*/
		{
		while ( l_k < 256 && isdigit(l_k) )
			{
			l_n = $MIN(l_n * 10 + (l_k - '0'), TPU$K_PBSZ / 2);
			snprintf(l_lbl, sizeof(l_lbl), "Repeat %d: ", l_n);
			s_prompt_label = l_lbl;
			s_prompt_text = "";
			s_scr_render();
			l_k = s_key_get();
			}

		s_prompt_label = NULL;

		if ( s_is_gold(l_k) )
			l_k = s_key_get() | TPU$M_GOLD;

		{
		int	l_seq[l_n > 0 ? l_n : 1];

		for ( int l_i = 0; l_i < l_n; l_i++ )
			l_seq[l_i] = l_k;

		return	l_n ? s_pb_push_front(l_seq, l_n) : STS$K_SUCCESS;
		}
		}

	return	s_dispatch(l_k | TPU$M_GOLD);
}

/*
 *  DESCRIPTION: DO: prompt for a command line, look it up and run it. An empty line does nothing
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  RETURNS: the status of the command; STS$K_ERROR when the prompt was cancelled or the command not found
 */
static int	s_cmd_do (
	char	*a_arg
	)
{
char	l_line[256];
const TPU$CMD *l_cmd;
char *	l_carg;

	if ( TPU$FAILED(s_prompt("Command: ", l_line, sizeof(l_line))) )
		return	STS$K_ERROR;

	if ( !*l_line )
		return	STS$K_SUCCESS;

	if ( TPU$FAILED(s_cmd_lookup(l_line, &l_cmd, &l_carg)) )
		return	STS$K_ERROR;

	return	l_cmd->fn(l_carg);
}

/*
 *  DESCRIPTION: BUFFER name: show the named buffer in the current window, creating an empty scratch buffer if
 *  there is none. Without a name: back to the window's previous buffer
 *
 *  INPUTS:
 *	a_arg		the name, prompted for if empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_buffers	a buffer may be created
 *	s_win[s_curwin]	switched
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOPREVBUF, or STS$K_ERROR when the prompt was cancelled
 */
static int	s_cmd_buffer (
	char	*a_arg
	)
{
char	l_name[64];
TPU$BUFFER *l_b;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Buffer name: ", l_name, sizeof(l_name))) )
		return	STS$K_ERROR;

	if ( !*l_name )
		{
		if ( !CURWIN->prev )
			return	s_tpu_msg(TPU$_NOPREVBUF);
		l_b = CURWIN->prev;
		}
	else if ( !(l_b = s_buf_find(l_name)) )
		{
		l_b = s_buf_create(l_name, 0);
		s_tpu_msg(TPU$_BUFCREATED, l_b->name);
		}

	s_win_set_buf(CURWIN, l_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: GET FILE name: read a file into a buffer named after the file (without the directory) and
 *  show it; an existing buffer of that name is shown as is
 *
 *  INPUTS:
 *	a_arg		the file name, prompted for if empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_buffers	a buffer may be created
 *	s_win[s_curwin]	switched
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR when the file cannot be read or the prompt was cancelled
 */
static int	s_cmd_get_file (
	char	*a_arg
	)
{
char	l_file[256];
const char *l_base;
TPU$BUFFER *l_b;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "File to get: ", l_file, sizeof(l_file))) || !*l_file )
		return	STS$K_ERROR;

	l_base = strrchr(l_file, '/');
	l_base = l_base ? l_base + 1 : l_file;

	if ( (l_b = s_buf_find(l_base)) )
		s_tpu_msg(TPU$_BUFEXISTS, l_b->name);
	else	{
		l_b = s_buf_create(l_base, 0);
		if ( TPU$FAILED(s_buf_load(l_b, l_file)) )
			{
			s_buf_destroy(l_b);
			return	STS$K_ERROR;
			}
		}

	s_win_set_buf(CURWIN, l_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: WRITE FILE [name]: write the current buffer to its file or to the named one, which then
 *  becomes the buffer's file; a buffer without a file is asked for a name
 *
 *  INPUTS:
 *	a_arg		the file name, may be empty
 *
 *  RETURNS: the status of s_buf_write(); TPU$_SYSBUF for a system buffer; STS$K_ERROR when the prompt was
 *  cancelled
 */
static int	s_cmd_write_file (
	char	*a_arg
	)
{
char	l_file[256];
TPU$BUFFER *l_b = CURBUF;

	if ( l_b->flags & BUF$M_SYSTEM )
		return	s_tpu_msg(TPU$_SYSBUF, l_b->name);

	if ( a_arg && *a_arg )
		snprintf(l_file, sizeof(l_file), "%s", a_arg);
	else if ( *l_b->file )
		snprintf(l_file, sizeof(l_file), "%s", l_b->file);
	else if ( TPU$FAILED(s_prompt("File to write: ", l_file, sizeof(l_file))) || !*l_file )
		return	STS$K_ERROR;

	return	s_buf_write(l_b, l_file);
}

/*
 *  DESCRIPTION: INCLUDE FILE name: insert a file at the cursor, through a temporary system buffer
 *
 *  INPUTS:
 *	a_arg		the file name, prompted for if empty
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_OPENIN, or STS$K_ERROR for a read-only buffer or a cancelled prompt
 */
static int	s_cmd_include_file (
	char	*a_arg
	)
{
char	l_file[256];
TPU$BUFFER *l_inc;

	if ( TPU$FAILED(s_buf_writable(CURBUF)) )
		return	STS$K_ERROR;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "File to include: ", l_file, sizeof(l_file))) || !*l_file )
		return	STS$K_ERROR;

	l_inc = s_buf_create("$INCLUDE", BUF$M_SYSTEM);

	if ( access(l_file, R_OK) || TPU$FAILED(s_buf_load(l_inc, l_file)) )
		{
		s_buf_destroy(l_inc);
		return	s_tpu_msg(TPU$_OPENIN, l_file, strerror(errno));
		}

	s_clip_insert(CURBUF, l_inc);
	s_buf_destroy(l_inc);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: DELETE BUFFER name: remove a buffer; a modified one asks for confirmation, system buffers are
 *  refused. Windows showing it fall back to another buffer
 *
 *  INPUTS:
 *	a_arg		the name, prompted for if empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_buffers, s_win[]	updated
 *
 *  RETURNS: TPU$_BUFDELETED, TPU$_NOSUCHBUF, TPU$_SYSBUF, TPU$_NOTDELETED, or STS$K_ERROR when the prompt was
 *  cancelled
 */
static int	s_cmd_delete_buffer (
	char	*a_arg
	)
{
char	l_name[64];
TPU$BUFFER *l_b;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Buffer to delete: ", l_name, sizeof(l_name))) || !*l_name )
		return	STS$K_ERROR;

	if ( !(l_b = s_buf_find(l_name)) )
		return	s_tpu_msg(TPU$_NOSUCHBUF, l_name);

	if ( l_b->flags & BUF$M_SYSTEM )
		return	s_tpu_msg(TPU$_SYSBUF, l_b->name);

	if ( (l_b->flags & BUF$M_MODIFIED) && s_yesno("Buffer is modified, delete anyway? [Y/N] ") != 1 )
		return	s_tpu_msg(TPU$_NOTDELETED, l_b->name);

	for ( int l_w = 0; l_w < 2; l_w++ )
		{
		if ( s_win[l_w].buf == l_b )
			s_win[l_w].buf = s_buf_fallback(l_b), s_win[l_w].top = s_win[l_w].left = 0;
		if ( s_win[l_w].prev == l_b )
			s_win[l_w].prev = NULL;
		}

	s_buf_destroy(l_b);

	return	s_tpu_msg(TPU$_BUFDELETED, l_name);
}

/*
 *  DESCRIPTION: SHOW BUFFERS: build the $BUFFERS listing (name, lines, file, state) and show it until a
 *  non-navigation key
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	the $BUFFERS system buffer	rebuilt
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_cmd_show_buffers (
	char	*a_arg
	)
{
TPU$BUFFER *l_list = s_buf_find("$BUFFERS"), *l_b;
char	l_row[512];

	if ( !l_list )
		l_list = s_buf_create("$BUFFERS", BUF$M_SYSTEM | BUF$M_NOWRITE);

	s_buf_clear(l_list);
	snprintf(l_row, sizeof(l_row), "%-24s %8s  %s", "Buffer", "Lines", "File");
	s_buf_append_line(l_list, l_row, strlen(l_row));

	for ( l_b = s_buffers; l_b; l_b = l_b->next )
		{
		if ( l_b == l_list )
			continue;

		snprintf(l_row, sizeof(l_row), "%-24s %8u  %s%s%s%s", l_b->name, l_b->nlines, l_b->file,
			(l_b->flags & BUF$M_MODIFIED) ? " [modified]" : "",
			(l_b->flags & BUF$M_NOWRITE) ? " [read-only]" : "",
			(l_b->flags & BUF$M_SYSTEM) ? " [system]" : "");
		s_buf_append_line(l_list, l_row, strlen(l_row));
		}

	l_list->cur_line = l_list->cur_col = 0;

	return	s_show_sysbuf(l_list);
}

/*
 *  DESCRIPTION: LINE number: go to the start of a line; a number past the end goes to the last line
 *
 *  INPUTS:
 *	a_arg		the number, prompted for if empty
 *
 *  OUTPUTS:
 *	the current buffer	cursor moved
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_BADLINE, or STS$K_ERROR when the prompt was cancelled
 */
static int	s_cmd_line (
	char	*a_arg
	)
{
char	l_num[32];
long	l_n;
TPU$BUFFER *l_b = CURBUF;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Line number: ", l_num, sizeof(l_num))) )
		return	STS$K_ERROR;

	if ( (l_n = strtol(l_num, NULL, 10)) < 1 )
		return	s_tpu_msg(TPU$_BADLINE, l_num);

	l_b->cur_line = $MIN((unsigned) l_n, l_b->nlines) - 1;
	l_b->cur_col = 0;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: FIND and FIND NEXT: search from the cursor in the buffer's direction. An empty answer to the
 *  FIND prompt repeats the last search
 *
 *  INPUTS:
 *	a_arg		the string, prompted for if empty (FIND)
 *	NULL for FIND NEXT
 *	a_skip		non-zero to skip a match at the cursor itself (FIND NEXT)
 *
 *  OUTPUTS:
 *	the current buffer	cursor on the match
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_findstr	the last search string
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOSEARCH, or STS$K_ERROR when nothing was found or the prompt was cancelled
 */
static int	s_find_common (
	char	*a_arg,
	int	a_skip
	)
{
char	l_str[256];
TPU$BUFFER *l_b = CURBUF;
TPU$POS	l_from = {l_b->cur_line, l_b->cur_col}, l_found = {0, 0};

	if ( !a_skip )
		{
		if ( TPU$FAILED(s_arg_or_prompt(a_arg, l_b->reverse ? "Reverse find: " : "Forward find: ", l_str, sizeof(l_str))) )
			return	STS$K_ERROR;

		if ( *l_str )
			{
			s_findstr.len = $MIN(strlen(l_str), ASC$K_SZ);
			memcpy(s_findstr.sts, l_str, s_findstr.len);
			}
		else if ( !s_findstr.len )
			return	s_tpu_msg(TPU$_NOSEARCH);
		else	a_skip = 1;				/* Empty answer: FIND NEXT		*/
		}

	if ( TPU$FAILED(s_buf_search(l_b, &s_findstr, l_from, a_skip, &l_found)) )
		return	STS$K_ERROR;

	l_b->cur_line = l_found.line;
	l_b->cur_col = l_found.col;

	return	STS$K_SUCCESS;
}

/* FIND string: search from the cursor in the buffer's direction. a_arg - the string, prompted for if empty; RETURNS the status of s_find_common() */
static int	s_cmd_find (
	char	*a_arg
	)
{
	return	s_find_common(a_arg, 0);
}

/*
 *  DESCRIPTION: WILDCARD FIND pattern: find the next line, in the buffer's direction, that matches a
 *  VMS-style pattern as a whole: * is any string, % any single character. A pattern in lower case ignores
 *  case. The cursor goes to the start of the line
 *
 *  INPUTS:
 *	a_arg		the pattern, prompted for if empty
 *
 *  OUTPUTS:
 *	the current buffer	cursor moved
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOMATCHLINE, or STS$K_ERROR when the prompt was cancelled
 */
static int	s_cmd_wildcard_find (
	char	*a_arg
	)
{
char	l_pat[256], l_lc[4096];
TPU$BUFFER *l_b = CURBUF;
int	l_ci, l_n;
ASC	l_asc;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, l_b->reverse ? "Reverse wildcard find: " : "Forward wildcard find: ", l_pat, sizeof(l_pat))) || !*l_pat )
		return	STS$K_ERROR;

	l_asc.len = $MIN(strlen(l_pat), ASC$K_SZ);
	memcpy(l_asc.sts, l_pat, l_asc.len);

	if ( (l_ci = s_pat_ci(&l_asc)) )
		{
		char	l_tmp[sizeof(l_pat)];

		s_utf8_lower(l_pat, strlen(l_pat), l_tmp, sizeof(l_tmp));
		strcpy(l_pat, l_tmp);
		}

	for ( int l_line = l_b->cur_line + (l_b->reverse ? -1 : 1); l_line >= 0 && l_line < (int) l_b->nlines; l_line += l_b->reverse ? -1 : 1 )
		{
		l_n = $MIN(l_b->lines[l_line]->len, sizeof(l_lc) / 2);

		if ( l_ci )
			s_utf8_lower(l_b->lines[l_line]->text, l_n, l_lc, sizeof(l_lc));
		else	memcpy(l_lc, l_b->lines[l_line]->text, l_n), l_lc[l_n] = '\0';

		if ( __util$pattern_match(l_lc, l_pat) )
			{
			l_b->cur_line = l_line;
			l_b->cur_col = 0;
			return	STS$K_SUCCESS;
			}
		}

	return	s_tpu_msg(TPU$_NOMATCHLINE, l_pat);
}
/* FIND NEXT: search again for the last string, skipping a match at the cursor. RETURNS the status of s_find_common() */
static int	s_cmd_find_next (
	char	*a_arg
	)
{
	return	s_find_common(NULL, 1);
}

/*
 *  DESCRIPTION: REPLACE old: prompt for the new string, then stop at each match and ask Yes, No, All, Last or
 *  Quit; All replaces the rest without asking, Last replaces this one and stops. Searches go in the buffer's
 *  direction from the cursor
 *
 *  INPUTS:
 *	a_arg		the old string, prompted for if empty
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  RETURNS: TPU$_REPLACED with the count, or STS$K_ERROR for a read-only buffer or a cancelled prompt
 */
static int	s_cmd_replace (
	char	*a_arg
	)
{
char	l_olds[256], l_news[256];
ASC	l_old;
TPU$BUFFER *l_b = CURBUF;
TPU$POS	l_from, l_found = {0, 0};
int	l_skip = 0, l_all = 0, l_count = 0, l_k, l_c;
unsigned l_newlen;

	if ( TPU$FAILED(s_buf_writable(l_b)) )
		return	STS$K_ERROR;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Old string: ", l_olds, sizeof(l_olds))) || !*l_olds )
		return	STS$K_ERROR;

	if ( TPU$FAILED(s_prompt("New string: ", l_news, sizeof(l_news))) )
		return	STS$K_ERROR;

	l_old.len = $MIN(strlen(l_olds), ASC$K_SZ);
	memcpy(l_old.sts, l_olds, l_old.len);
	l_newlen = strlen(l_news);

	l_from.line = l_b->cur_line;
	l_from.col = l_b->cur_col;

	for (;;)
		{
		if ( TPU$FAILED(s_buf_search(l_b, &l_old, l_from, l_skip, &l_found)) )
			break;

		l_b->cur_line = l_found.line;
		l_b->cur_col = l_found.col;

		if ( !l_all )
			{
			l_k = s_key_ask("Replace? Type Yes, No, All, Last, or Quit: ");
			l_c = tolower(l_k & 0xff);

			if ( l_k >= 256 || l_c == 'q' )
				break;

			if ( l_c == 'n' )
				{
				l_from = l_found;
				l_skip = 1;
				continue;
				}

			if ( l_c == 'a' )
				l_all = 1;
			else if ( l_c != 'y' && l_c != 'l' )
				continue;
			}

		s_line_del(l_b->lines[l_found.line], l_found.col, s_matchlen);
		s_line_ins(l_b->lines[l_found.line], l_found.col, l_news, l_newlen);
		s_buf_touch(l_b);
		l_count++;

		l_b->cur_col = l_found.col + l_newlen;
		l_from.line = l_b->cur_line;
		l_from.col = l_b->cur_col;
		l_skip = 0;

		if ( !l_all && l_c == 'l' )
			break;
		}

	return	s_tpu_msg(TPU$_REPLACED, l_count, l_count == 1 ? "" : "s");
}

/*
 *  DESCRIPTION: SELECT: put the selection anchor at the cursor, or cancel an active selection
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	mark_line, mark_col
 *
 *  RETURNS: TPU$_SELECT or TPU$_SELCANCEL
 */
static int	s_cmd_select (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;

	if ( l_b->mark_line >= 0 )
		{
		l_b->mark_line = l_b->mark_col = -1;
		return	s_tpu_msg(TPU$_SELCANCEL);
		}

	l_b->mark_line = l_b->cur_line;
	l_b->mark_col = $MIN(l_b->cur_col, CURLINE(l_b)->len);

	return	s_tpu_msg(TPU$_SELECT);
}

/*
 *  DESCRIPTION: RESET: cancel the selection and clear the message line
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	selection cancelled
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_cmd_reset (
	char	*a_arg
	)
{
	CURBUF->mark_line = CURBUF->mark_col = -1;
	s_msg[0] = '\0';
	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: REMOVE: cut the selection into the INSERT HERE buffer
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_pastebuf	the removed text
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer or no selection
 */
static int	s_cmd_remove (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
TPU$POS	l_from, l_to;

	if ( TPU$FAILED(s_buf_writable(l_b)) || TPU$FAILED(s_sel_range(l_b, &l_from, &l_to)) )
		return	STS$K_ERROR;

	s_range_copy(l_b, l_from, l_to, s_pastebuf);
	s_range_delete(l_b, l_from, l_to);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: STORE TEXT: copy the selection into the INSERT HERE buffer without deleting it, then cancel
 *  the selection
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_pastebuf	the copied text
 *
 *  RETURNS: TPU$_STORED, or STS$K_ERROR when there is no selection
 */
static int	s_cmd_store_text (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
TPU$POS	l_from, l_to;

	if ( TPU$FAILED(s_sel_range(l_b, &l_from, &l_to)) )
		return	STS$K_ERROR;

	s_range_copy(l_b, l_from, l_to, s_pastebuf);
	l_b->mark_line = l_b->mark_col = -1;

	return	s_tpu_msg(TPU$_STORED);
}

/*
 *  DESCRIPTION: Insert a buffer (INSERT HERE or $RESTORE) at the cursor of the current buffer
 *
 *  INPUTS:
 *	a_src		the buffer to insert
 *	a_what		"insert" or "restore" for the TPU$_NOTHING message
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOTHING when a_src is empty, or STS$K_ERROR for a read-only buffer
 */
static int	s_paste_from (
	TPU$BUFFER	*a_src,
	const char	*a_what
	)
{
	if ( TPU$FAILED(s_buf_writable(CURBUF)) )
		return	STS$K_ERROR;

	if ( a_src->nlines == 1 && !a_src->lines[0]->len )
		return	s_tpu_msg(TPU$_NOTHING, a_what);

	s_clip_insert(CURBUF, a_src);

	return	STS$K_SUCCESS;
}

/* INSERT HERE: paste the INSERT HERE buffer at the cursor. RETURNS the status of s_paste_from() */
static int	s_cmd_insert_here (
	char	*a_arg
	)
{
	return	s_paste_from(s_pastebuf, "insert");
}
/* RESTORE: put back the text of the last ERASE WORD / LINE / START OF LINE. RETURNS the status of s_paste_from() */
static int	s_cmd_restore (
	char	*a_arg
	)
{
	return	s_paste_from(s_restorebuf, "restore");
}

/* MOVE UP: cursor one line up. RETURNS STS$K_SUCCESS */
static int	s_cmd_move_up (
	char	*a_arg
	)
{
	return	s_mv_up(CURBUF, 1);
}
/* MOVE DOWN: cursor one line down. RETURNS STS$K_SUCCESS */
static int	s_cmd_move_down (
	char	*a_arg
	)
{
	return	s_mv_down(CURBUF, 1);
}
/* MOVE LEFT: cursor one character left, wrapping to the previous line. RETURNS STS$K_SUCCESS */
static int	s_cmd_move_left (
	char	*a_arg
	)
{
	return	s_mv_left(CURBUF);
}
/* MOVE RIGHT: cursor one character right, wrapping to the next line. RETURNS STS$K_SUCCESS */
static int	s_cmd_move_right (
	char	*a_arg
	)
{
	return	s_mv_right(CURBUF);
}
/* MOVE BY CHAR: one character in the current direction, wrapping over line ends (EDT's CHAR). RETURNS STS$K_SUCCESS */
static int	s_cmd_move_by_char (
	char	*a_arg
	)
{
	return	CURBUF->reverse ? s_mv_left(CURBUF) : s_mv_right(CURBUF);
}
/* MOVE BY LINE START: see s_mv_by_line_start(). RETURNS STS$K_SUCCESS */
static int	s_cmd_move_by_line_start (
	char	*a_arg
	)
{
	return	s_mv_by_line_start(CURBUF);
}
/* MOVE BY LINE: see s_mv_by_line(). RETURNS STS$K_SUCCESS */
static int	s_cmd_move_by_line (
	char	*a_arg
	)
{
	return	s_mv_by_line(CURBUF);
}
/*
 *  DESCRIPTION: PREVIOUS WORD and NEXT WORD: one word back or forward whatever the buffer's direction is. For
 *  a keyboard where FORWARD and REVERSE have no key of their own (a PC keypad on GOLD-Left / GOLD-Right)
 *
 *  INPUTS:
 *	a_reverse	non-zero for PREVIOUS WORD
 *
 *  OUTPUTS:
 *	the current buffer	cursor moved; its direction is left as it was
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_word_step (
	int	a_reverse
	)
{
int	l_saved = CURBUF->reverse, l_sts;

	CURBUF->reverse = a_reverse;
	l_sts = s_mv_by_word(CURBUF);
	CURBUF->reverse = l_saved;

	return	l_sts;
}

/* PREVIOUS WORD: one word back, direction untouched. RETURNS STS$K_SUCCESS */
static int	s_cmd_previous_word (
	char	*a_arg
	)
{
	return	s_word_step(1);
}

/* NEXT WORD: one word forward, direction untouched. RETURNS STS$K_SUCCESS */
static int	s_cmd_next_word (
	char	*a_arg
	)
{
	return	s_word_step(0);
}

/* MOVE BY WORD: see s_mv_by_word(). RETURNS STS$K_SUCCESS */
static int	s_cmd_move_by_word (
	char	*a_arg
	)
{
	return	s_mv_by_word(CURBUF);
}

/* START OF LINE: cursor to column 0. RETURNS STS$K_SUCCESS */
static int	s_cmd_start_of_line (
	char	*a_arg
	)
{
	CURBUF->cur_col = 0;
	return	STS$K_SUCCESS;
}
/* END OF LINE: cursor after the last character of the line. RETURNS STS$K_SUCCESS */
static int	s_cmd_end_of_line (
	char	*a_arg
	)
{
	CURBUF->cur_col = CURLINE(CURBUF)->len;
	return	STS$K_SUCCESS;
}
/* TOP: cursor to the start of the buffer. RETURNS STS$K_SUCCESS */
static int	s_cmd_top (
	char	*a_arg
	)
{
	CURBUF->cur_line = CURBUF->cur_col = 0;
	return	STS$K_SUCCESS;
}
/* BOTTOM: cursor to the end of the buffer. RETURNS STS$K_SUCCESS */
static int	s_cmd_bottom (
	char	*a_arg
	)
{
	CURBUF->cur_line = CURBUF->nlines - 1;
	CURBUF->cur_col = CURLINE(CURBUF)->len;
	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: NEXT SCREEN: scroll the current window one screen forward, moving the cursor with it. The
 *  window top never goes past the point where the [End of file] marker is its last row, so a short buffer
 *  does not scroll out of sight
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current window and buffer	top and cursor moved
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_ENDOFBUF
 */
static int	s_cmd_next_screen (
	char	*a_arg
	)
{
TPU$WINDOW *l_w = CURWIN;
unsigned l_maxtop;
int	l_n = $MAX(1, l_w->nrows - 1);

	if ( l_w->buf->cur_line + 1 >= l_w->buf->nlines )
		return	s_tpu_msg(TPU$_ENDOFBUF);

	/* Last useful top: the [End of file] marker (row nlines) on the last row of the window */
	l_maxtop = (l_w->buf->nlines + 1 > (unsigned) l_w->nrows) ? l_w->buf->nlines + 1 - l_w->nrows : 0;
	l_w->top = $MIN(l_w->top + l_n, l_maxtop);

	return	s_mv_down(l_w->buf, l_n);
}

/*
 *  DESCRIPTION: PREVIOUS SCREEN: scroll the current window one screen back, moving the cursor with it
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current window and buffer	top and cursor moved
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_TOPOFBUF
 */
static int	s_cmd_prev_screen (
	char	*a_arg
	)
{
TPU$WINDOW *l_w = CURWIN;
int	l_n = $MAX(1, l_w->nrows - 1);

	if ( !l_w->buf->cur_line )
		return	s_tpu_msg(TPU$_TOPOFBUF);

	l_w->top = (l_w->top > (unsigned) l_n) ? l_w->top - l_n : 0;
	return	s_mv_up(l_w->buf, l_n);
}

/* FORWARD: direction for FIND, MOVE BY LINE and MOVE BY WORD. RETURNS STS$K_SUCCESS */
static int	s_cmd_forward (
	char	*a_arg
	)
{
	CURBUF->reverse = 0;
	return	STS$K_SUCCESS;
}
/* REVERSE: the opposite direction. RETURNS STS$K_SUCCESS */
static int	s_cmd_reverse (
	char	*a_arg
	)
{
	CURBUF->reverse = 1;
	return	STS$K_SUCCESS;
}
/* INSERT MODE: typed text is inserted. RETURNS STS$K_SUCCESS */
static int	s_cmd_insert_mode (
	char	*a_arg
	)
{
	CURBUF->overstrike = 0;
	return	STS$K_SUCCESS;
}
/* OVERSTRIKE MODE: typed text replaces what is under the cursor. RETURNS STS$K_SUCCESS */
static int	s_cmd_overstrike_mode (
	char	*a_arg
	)
{
	CURBUF->overstrike = 1;
	return	STS$K_SUCCESS;
}
/* CHANGE MODE: toggle insert / overstrike. RETURNS STS$K_SUCCESS */
static int	s_cmd_change_mode (
	char	*a_arg
	)
{
	CURBUF->overstrike = !CURBUF->overstrike;
	return	STS$K_SUCCESS;
}

/* RETURN: split the line at the cursor. RETURNS the status of s_ed_newline() */
static int	s_cmd_return (
	char	*a_arg
	)
{
	return	s_ed_newline(CURBUF);
}
/* TAB: insert a TAB character, expanded on display only. RETURNS the status of s_ed_insert_char() */
static int	s_cmd_tab (
	char	*a_arg
	)
{
	return	s_ed_insert_char(CURBUF, '\t');
}
/* ERASE PREVIOUS: the character before the cursor. RETURNS the status of s_ed_erase_prev() */
static int	s_cmd_erase_previous (
	char	*a_arg
	)
{
	return	s_ed_erase_prev(CURBUF);
}
/* ERASE CHARACTER: the character under the cursor. RETURNS the status of s_ed_erase_char() */
static int	s_cmd_erase_char (
	char	*a_arg
	)
{
	return	s_ed_erase_char(CURBUF);
}
/* ERASE WORD: from the cursor to the end of the word. RETURNS the status of s_ed_erase_word() */
static int	s_cmd_erase_word (
	char	*a_arg
	)
{
	return	s_ed_erase_word(CURBUF);
}
/* ERASE START OF LINE: from the start of the line to the cursor. RETURNS the status of s_ed_erase_sol() */
static int	s_cmd_erase_sol (
	char	*a_arg
	)
{
	return	s_ed_erase_sol(CURBUF);
}

/*
 *  DESCRIPTION: ERASE END OF LINE: from the cursor to the end of the line, the line break stays (EDT's DEL EOL);
 *  the text is kept for RESTORE
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_restorebuf	the erased text
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_cmd_erase_eol (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
TPU$LINE *	l_l = CURLINE(l_b);
unsigned l_col = $MIN(l_b->cur_col, l_l->len);

	if ( TPU$FAILED(s_buf_writable(l_b)) )
		return	STS$K_ERROR;

	if ( l_col == l_l->len )
		return	STS$K_SUCCESS;

	s_ed_save_restore(l_l->text + l_col, l_l->len - l_col);
	l_l->len = l_col;
	l_b->cur_col = l_col;
	s_buf_touch(l_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: OPEN LINE: break the line at the cursor and leave the cursor where it was, before the break
 *  (EDT's OPEN LINE)
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  RETURNS: the status of s_ed_newline()
 */
static int	s_cmd_open_line (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
unsigned l_line = l_b->cur_line, l_col = $MIN(l_b->cur_col, CURLINE(l_b)->len);
int	l_status;

	if ( TPU$FAILED(l_status = s_ed_newline(l_b)) )
		return	l_status;

	l_b->cur_line = l_line;
	l_b->cur_col = l_col;

	return	l_status;
}

/*
 *  DESCRIPTION: Swap the case of the characters of one line between two byte offsets; a character whose other
 *  case is longer or shorter in UTF-8 is spliced in, so the end offset may move
 *
 *  INPUTS:
 *	a_l		the line
 *	a_from		first byte
 *	a_to		byte after the last one
 *
 *  OUTPUTS:
 *	a_l		modified
 *
 *  RETURNS: the end offset after the change
 */
static unsigned	s_case_swap (
	TPU$LINE	*a_l,
	unsigned	a_from,
	unsigned	a_to
	)
{
char	l_enc[8];
int	l_n, l_m;
wchar_t	l_wc;

	for ( unsigned l_i = a_from; l_i < $MIN(a_to, a_l->len); l_i += l_m )
		{
		l_n = s_utf8_len(a_l->text + l_i, a_l->len - l_i);
		l_wc = s_utf8_decode(a_l->text + l_i, l_n);
		l_wc = iswupper(l_wc) ? towlower(l_wc) : towupper(l_wc);
		l_m = s_utf8_encode(l_wc, l_enc);

		if ( l_m == l_n )
			memcpy(a_l->text + l_i, l_enc, l_m);
		else	{
			s_line_del(a_l, l_i, l_n);
			s_line_ins(a_l, l_i, l_enc, l_m);
			a_to = a_to + l_m - l_n;
			}
		}

	return	a_to;
}

/*
 *  DESCRIPTION: CHANGE CASE: swap upper and lower case in the selection and cancel it; with no selection, of the
 *  character under the cursor, then step one character in the current direction (EDT's CHNGCASE)
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_cmd_change_case (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
TPU$LINE *	l_l = CURLINE(l_b);
TPU$POS	l_from, l_to;

	if ( TPU$FAILED(s_buf_writable(l_b)) )
		return	STS$K_ERROR;

	if ( s_sel_bounds(l_b, &l_from, &l_to) )
		{
		for ( unsigned l_i = l_from.line; l_i <= l_to.line; l_i++ )
			s_case_swap(l_b->lines[l_i], l_i == l_from.line ? l_from.col : 0,
				l_i == l_to.line ? l_to.col : l_b->lines[l_i]->len);

		l_b->mark_line = l_b->mark_col = -1;
		s_buf_touch(l_b);

		return	STS$K_SUCCESS;
		}

	if ( l_b->cur_col < l_l->len )
		{
		unsigned l_col = l_b->cur_col;

		l_b->cur_col = s_case_swap(l_l, l_col, l_col + s_utf8_len(l_l->text + l_col, l_l->len - l_col));
		s_buf_touch(l_b);

		if ( l_b->reverse )
			{
			l_b->cur_col = l_col;
			s_mv_left(l_b);
			}
		}
	else	s_mv_right(l_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: ERASE LINE: delete the whole current line, saving it for RESTORE
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  OUTPUTS:
 *	the current buffer	modified
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_restorebuf	the erased line
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR for a read-only buffer
 */
static int	s_cmd_erase_line (
	char	*a_arg
	)
{
TPU$BUFFER *l_b = CURBUF;
TPU$LINE *	l_l = CURLINE(l_b);

	if ( TPU$FAILED(s_buf_writable(l_b)) )
		return	STS$K_ERROR;

	s_ed_save_restore(l_l->text, l_l->len);
	s_buf_del_line(l_b, l_b->cur_line);
	l_b->cur_line = $MIN(l_b->cur_line, l_b->nlines - 1);
	l_b->cur_col = 0;
	s_buf_touch(l_b);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: ONE WINDOW: keep only the current window, full height
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_win[], s_nwin, s_curwin	updated
 *	the screen is laid out again
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_ONEWINDOW
 */
static int	s_cmd_one_window (
	char	*a_arg
	)
{
	if ( s_nwin == 1 )
		return	s_tpu_msg(TPU$_ONEWINDOW);

	s_win[0] = s_win[s_curwin];
	s_curwin = 0;
	s_nwin = 1;
	s_scr_relayout();

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: TWO WINDOWS: split the screen, both windows showing the current buffer at first
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_win[], s_nwin	updated
 *	the screen is laid out again
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_TWOWINDOWS
 */
static int	s_cmd_two_windows (
	char	*a_arg
	)
{
	if ( s_nwin == 2 )
		return	s_tpu_msg(TPU$_TWOWINDOWS);

	s_win[1] = s_win[0];
	s_nwin = 2;
	s_scr_relayout();

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: OTHER WINDOW: move the cursor to the other window
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_curwin	toggled
 *
 *  RETURNS: STS$K_SUCCESS or TPU$_ONLYONEWIN
 */
static int	s_cmd_other_window (
	char	*a_arg
	)
{
	if ( s_nwin == 1 )
		return	s_tpu_msg(TPU$_ONLYONEWIN);

	s_curwin = !s_curwin;

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: REFRESH: re-read the screen size and repaint everything, for a serial line that cannot signal
 *  a resize
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_rows, s_cols, s_win[]	updated
 *	the screen is invalidated
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_cmd_refresh (
	char	*a_arg
	)
{
	s_scr_getsize();
	s_scr_relayout();

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: LEARN: start recording every key until REMEMBER
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_learning, s_learn_len	recording starts
 *
 *  RETURNS: TPU$_LEARNING or TPU$_ALRLEARN
 */
static int	s_cmd_learn (
	char	*a_arg
	)
{
	if ( s_learning )
		return	s_tpu_msg(TPU$_ALRLEARN);

	s_learning = 1;
	s_learn_len = 0;

	return	s_tpu_msg(TPU$_LEARNING);
}

/*
 *  DESCRIPTION: REMEMBER: stop recording, drop the key that invoked REMEMBER itself, ask for a key and bind
 *  the sequence to it. Return, ESC or Ctrl-Z at the question discards the sequence
 *
 *  INPUTS:
 *	a_arg		ignored
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_learning	cleared
 *	s_keydefs	a definition added or replaced
 *
 *  RETURNS: TPU$_KEYLEARNED, TPU$_NOTLEARN, TPU$_NOLEARN, TPU$_DISCARDED or TPU$_KEYTBLFULL
 */
static int	s_cmd_remember (
	char	*a_arg
	)
{
int	l_k, l_len;
char	l_name[32];

	if ( !s_learning )
		return	s_tpu_msg(TPU$_NOTLEARN);

	s_learning = 0;
	l_len = $MAX(s_learn_mark - 1, 0);			/* Drop the key that invoked REMEMBER	*/

	if ( !l_len )
		return	s_tpu_msg(TPU$_NOLEARN);

	l_k = s_key_ask("Press the key you want to use for the learned sequence: ");

	if ( l_k == 13 || l_k == 0x1a )			/* Not ESC: that is GOLD on a PC keyboard	*/
		return	s_tpu_msg(TPU$_DISCARDED);

	if ( !s_keydef_set(l_k, KD$K_MACRO, NULL, s_learn, l_len) )
		return	s_tpu_msg(TPU$_KEYTBLFULL);

	return	s_tpu_msg(TPU$_KEYLEARNED, s_key_name(l_k, l_name, sizeof(l_name)), l_len);
}

/*
 *  DESCRIPTION: REPEAT count: ask for a key and queue it that many times
 *
 *  INPUTS:
 *	a_arg		the count, prompted for if empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	the playback queue
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_BADCOUNT, TPU$_QUEOVF, or STS$K_ERROR when the prompt was cancelled
 */
static int	s_cmd_repeat (
	char	*a_arg
	)
{
char	l_num[32];
long	l_n;
int	l_k;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Repeat count: ", l_num, sizeof(l_num))) )
		return	STS$K_ERROR;

	if ( (l_n = strtol(l_num, NULL, 10)) < 1 || l_n > TPU$K_PBSZ / 2 )
		return	s_tpu_msg(TPU$_BADCOUNT, l_num);

	l_k = s_key_ask("Press the key to repeat: ");

	{
	int	l_seq[l_n];

	for ( int l_i = 0; l_i < l_n; l_i++ )
		l_seq[l_i] = l_k;

	return	s_pb_push_front(l_seq, l_n);
	}
}

/*
 *  DESCRIPTION: DEFINE KEY command: look the command up, ask for a key and bind them
 *
 *  INPUTS:
 *	a_arg		the command name, prompted for if empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_keydefs	a definition added or replaced
 *
 *  RETURNS: TPU$_KEYDEFINED, TPU$_KEYTBLFULL, or STS$K_ERROR when the command is unknown or the prompt was
 *  cancelled
 */
static int	s_cmd_define_key (
	char	*a_arg
	)
{
char	l_line[128], l_name[32];
const TPU$CMD *l_cmd;
char *	l_carg;
int	l_k;

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Command to define key for: ", l_line, sizeof(l_line))) || !*l_line )
		return	STS$K_ERROR;

	if ( TPU$FAILED(s_cmd_lookup(l_line, &l_cmd, &l_carg)) )
		return	STS$K_ERROR;

	l_k = s_key_ask("Press the key to define: ");

	if ( !s_keydef_set(l_k, KD$K_CMD, l_cmd, NULL, 0) )
		return	s_tpu_msg(TPU$_KEYTBLFULL);

	return	s_tpu_msg(TPU$_KEYDEFINED, s_key_name(l_k, l_name, sizeof(l_name)), l_cmd->name);
}

/*
 *  DESCRIPTION: Build the $KEYS listing of the live key table (key name and command, or "(learned sequence)")
 *  and show it until a non-navigation key is pressed. Behind SHOW KEYS, HELP KEYS, GOLD-K and GOLD-F1
 *
 *  IMPLICIT INPUTS:
 *	s_keydefs, s_nkeydefs	the key table
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	the $KEYS system buffer	rebuilt
 *
 *  RETURNS: STS$K_SUCCESS
 */
static int	s_show_keys (
	void
	)
{
TPU$BUFFER *l_keys;
TPU$KEYDEF *l_kd;
char	l_row[512], l_name[32], l_text[256];
char	l_items[TPU$K_MAXKEYDEF][64];
const char *l_ptrs[TPU$K_MAXKEYDEF];
int	l_n = 0, l_nmacro = 0, l_maxkey = 1;

	if ( !(l_keys = s_buf_find("$KEYS")) )
		l_keys = s_buf_create("$KEYS", BUF$M_SYSTEM | BUF$M_NOWRITE);

	s_buf_clear(l_keys);

	for ( int l_k = 0; l_k < s_nkeydefs; l_k++ )		/* Commands in columns, key name padded	*/
		if ( s_keydefs[l_k].type == KD$K_CMD )
			l_maxkey = $MAX(l_maxkey, (int) strlen(s_key_name(s_keydefs[l_k].key, l_name, sizeof(l_name))));
		else	l_nmacro++;

	for ( int l_k = 0; l_k < s_nkeydefs; l_k++ )
		{
		l_kd = &s_keydefs[l_k];

		if ( l_kd->type != KD$K_CMD )
			continue;

		snprintf(l_items[l_n], sizeof(l_items[0]), "%-*s %s", l_maxkey, s_key_name(l_kd->key, l_name, sizeof(l_name)), l_kd->cmd->name);
		l_ptrs[l_n] = l_items[l_n];
		l_n++;
		}

	snprintf(l_row, sizeof(l_row), "Key bindings: %d, learned sequences: %d. SAVE KEYS keeps them.", l_n, l_nmacro);
	s_buf_append_line(l_keys, l_row, strlen(l_row));
	s_buf_append_line(l_keys, "", 0);
	s_buf_columns(l_keys, l_ptrs, l_n);

	if ( l_nmacro )
		s_buf_append_line(l_keys, "", 0);

	for ( int l_k = 0; l_k < s_nkeydefs; l_k++ )		/* Learned sequences, one per line	*/
		{
		l_kd = &s_keydefs[l_k];

		if ( l_kd->type != KD$K_MACRO )
			continue;

		s_macro_text(l_kd->seq, l_kd->seqlen, l_text, sizeof(l_text));
		snprintf(l_row, sizeof(l_row), "  %-*s KEYS \"%s\"", l_maxkey, s_key_name(l_kd->key, l_name, sizeof(l_name)), l_text);
		s_buf_append_line(l_keys, l_row, strlen(l_row));
		}

	l_keys->cur_line = l_keys->cur_col = 0;

	return	s_show_sysbuf(l_keys);
}

/*
 *  DESCRIPTION: LOAD KEYS [file]: read a key file over the live table, see s_keys_load()
 *
 *  INPUTS:
 *	a_arg		the file, prompted for if empty
 *
 *  RETURNS: the status of s_keys_load(), or STS$K_ERROR when the prompt was cancelled or left empty
 */
static int	s_cmd_load_keys (
	char	*a_arg
	)
{
char	l_file[256];

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Key file to load: ", l_file, sizeof(l_file))) || !*l_file )
		return	STS$K_ERROR;

	return	s_keys_load(l_file, 0);
}

/*
 *  DESCRIPTION: SAVE KEYS [file]: write the live table as a key file, tpu.keys in the current directory by
 *  default, see s_keys_save()
 *
 *  INPUTS:
 *	a_arg		the file, prompted for if empty; an empty answer takes tpu.keys
 *
 *  RETURNS: the status of s_keys_save(), or STS$K_ERROR when the prompt was cancelled
 */
static int	s_cmd_save_keys (
	char	*a_arg
	)
{
char	l_file[256];

	if ( TPU$FAILED(s_arg_or_prompt(a_arg, "Key file to write [tpu.keys]: ", l_file, sizeof(l_file))) )
		return	STS$K_ERROR;

	if ( !*l_file )
		snprintf(l_file, sizeof(l_file), "%s", TPU$K_KEYFILE);

	return	s_keys_save(l_file);
}

/* HINTS: toggle the key hint line and lay the screen out again. RETURNS TPU$_HINTSON or TPU$_HINTSOFF */
static int	s_cmd_hints (
	char	*a_arg
	)
{
	s_hints = !s_hints;
	s_scr_relayout();

	return	s_tpu_msg(s_hints ? TPU$_HINTSON : TPU$_HINTSOFF);
}

/* SHOW KEYS: the live key table. RETURNS the status of s_show_keys() */
static int	s_cmd_show_keys (
	char	*a_arg
	)
{
	return	s_show_keys();
}

#define	TPU$K_MAXTOPICS	96
#define	TPU$K_TOPICSZ	48

typedef	struct	helptopic_t {
	char		name[TPU$K_TOPICSZ];			/* Upper case, as written after "1 "	*/
	unsigned	first, last;				/* Text lines of the topic, [first, last)	*/
} TPU$HELPTOPIC;

/*
 *  DESCRIPTION: Split the embedded help (VMS .HLP layout, "1 TOPIC" headers) into topics
 *
 *  INPUTS:
 *	a_max		room in a_t
 *
 *  OUTPUTS:
 *	a_t		the topics in file order
 *
 *  IMPLICIT INPUTS:
 *	s_helpbuf	the help text
 *
 *  RETURNS: number of topics
 */
static int	s_help_topics (
	TPU$HELPTOPIC	*a_t,
	int		a_max
	)
{
int	l_n = 0;
TPU$LINE *l_l;

	for ( unsigned l_i = 0; l_i < s_helpbuf->nlines; l_i++ )
		{
		l_l = s_helpbuf->lines[l_i];

		if ( l_l->len > 2 && l_l->text[0] == '1' && l_l->text[1] == ' ' )
			{
			if ( l_n )
				a_t[l_n - 1].last = l_i;

			if ( l_n == a_max )
				break;

			snprintf(a_t[l_n].name, sizeof(a_t[l_n].name), "%.*s", (int) $MIN(l_l->len - 2, sizeof(a_t[l_n].name) - 1), l_l->text + 2);
			a_t[l_n].first = l_i + 1;
			a_t[l_n].last = s_helpbuf->nlines;
			l_n++;
			}
		}

	return	l_n;
}

/* qsort() order for topic names */
static int	s_topic_cmp (
	const void	*a_a,
	const void	*a_b
	)
{
	return	strcasecmp(*(const char * const *) a_a, *(const char * const *) a_b);
}

/*
 *  DESCRIPTION: Build the $HELP buffer for a topic, VMS HELP style. No topic (or TPU): the introduction and
 *  "Additional information available:" with the topic names in columns. A topic: its name and text; an
 *  abbreviation is accepted while it is unambiguous, otherwise the candidates are listed; an unknown one
 *  gets "Sorry, no documentation on ..." and the topic list
 *
 *  INPUTS:
 *	a_topic		the topic, may be empty
 *
 *  OUTPUTS:
 *	a_hb		receives the buffer, created on first use
 *	a_list		non-zero when the page carries the topic list (the top level or a "Sorry" page)
 *
 *  IMPLICIT INPUTS:
 *	s_helpbuf	the help text
 *
 *  RETURNS: STS$K_SUCCESS, or TPU$_NOHELP (the message is shown) for an unknown or ambiguous topic
 */
static int	s_help_build (
	const char	*a_topic,
	TPU$BUFFER	**a_hb,
	int		*a_list
	)
{
TPU$HELPTOPIC l_t[TPU$K_MAXTOPICS];
const char *l_names[TPU$K_MAXTOPICS];
TPU$BUFFER *l_hb;
int	l_n, l_nn = 0, l_found = -1, l_nmatch = 0, l_top;
unsigned l_last;
char	l_row[256];

	if ( !(l_hb = s_buf_find("$HELP")) )
		l_hb = s_buf_create("$HELP", BUF$M_SYSTEM | BUF$M_NOWRITE);

	*a_hb = l_hb;
	*a_list = 0;
	s_buf_clear(l_hb);
	l_n = s_help_topics(l_t, TPU$K_MAXTOPICS);
	l_top = !*a_topic || !strcasecmp(a_topic, "TPU");

	for ( int l_i = 0; l_i < l_n; l_i++ )		/* The topic asked for, exact name first	*/
		if ( !strcasecmp(l_t[l_i].name, l_top ? "TPU" : a_topic) )
			{
			l_found = l_i, l_nmatch = 1;
			break;
			}

	if ( l_found < 0 && !l_top )
		for ( int l_i = 0; l_i < l_n; l_i++ )
			if ( !strncasecmp(l_t[l_i].name, a_topic, strlen(a_topic)) )
				{
				l_found = l_i;
				l_names[l_nmatch++] = l_t[l_i].name;
				}

	if ( l_found >= 0 && l_nmatch == 1 )
		{
		if ( !l_top )
			{
			s_buf_append_line(l_hb, l_t[l_found].name, strlen(l_t[l_found].name));
			s_buf_append_line(l_hb, "", 0);
			}

		for ( l_last = l_t[l_found].last; l_last > l_t[l_found].first && !s_helpbuf->lines[l_last - 1]->len; l_last-- );

		for ( unsigned l_i = l_t[l_found].first; l_i < l_last; l_i++ )
			s_buf_append_line(l_hb, s_helpbuf->lines[l_i]->text, s_helpbuf->lines[l_i]->len);

		if ( !l_top )
			return	STS$K_SUCCESS;
		}
	else if ( l_nmatch > 1 )
		{
		snprintf(l_row, sizeof(l_row), "%s is ambiguous, it could be:", a_topic);
		s_buf_append_line(l_hb, l_row, strlen(l_row));
		s_buf_append_line(l_hb, "", 0);
		qsort(l_names, l_nmatch, sizeof(l_names[0]), s_topic_cmp);
		s_buf_columns(l_hb, l_names, l_nmatch);

		return	s_tpu_msg(TPU$_NOHELP, a_topic);
		}
	else	{
		snprintf(l_row, sizeof(l_row), "Sorry, no documentation on %s", a_topic);
		s_buf_append_line(l_hb, l_row, strlen(l_row));
		}

	/* The top level: the list of topics in columns */
	*a_list = 1;
	s_buf_append_line(l_hb, "", 0);
	s_buf_append_line(l_hb, "Additional information available:", 33);
	s_buf_append_line(l_hb, "", 0);

	for ( int l_i = 0; l_i < l_n; l_i++ )
		if ( strcasecmp(l_t[l_i].name, "TPU") )
			l_names[l_nn++] = l_t[l_i].name;

	qsort(l_names, l_nn, sizeof(l_names[0]), s_topic_cmp);
	s_buf_columns(l_hb, l_names, l_nn);

	return	(l_found >= 0) ? STS$K_SUCCESS : s_tpu_msg(TPU$_NOHELP, a_topic);
}

/*
 *  DESCRIPTION: HELP [topic], VMS HELP style: the introduction with the topics in columns, or one topic, then
 *  the "Topic? " prompt: a name shows that topic, Return at a topic goes back to the list, Return at the list
 *  leaves. HELP KEYS shows the live key table instead, see s_show_keys()
 *
 *  INPUTS:
 *	a_arg		the topic, may be empty
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	the $HELP system buffer	rebuilt for every page
 *
 *  RETURNS: STS$K_SUCCESS, TPU$_NOHELP when the topic asked for was not found, or TPU$_CANCEL
 */
static int	s_cmd_help (
	char	*a_arg
	)
{
TPU$BUFFER *l_hb;
char	l_topic[TPU$K_TOPICSZ], l_ans[TPU$K_TOPICSZ];
int	l_sts, l_first = STS$K_SUCCESS, l_list;

	if ( a_arg && *a_arg && !strncasecmp(a_arg, "KEYS", strlen(a_arg)) )
		return	s_show_keys();

	snprintf(l_topic, sizeof(l_topic), "%s", a_arg ? a_arg : "");

	for ( int l_page = 0; ; l_page++ )
		{
		l_sts = s_help_build(l_topic, &l_hb, &l_list);

		if ( !l_page )
			l_first = l_sts;			/* What the command itself reports	*/

		if ( TPU$FAILED(s_show_sysbuf_ask(l_hb, "Topic? ", l_ans, sizeof(l_ans))) )
			return	TPU$_CANCEL;

		if ( !*l_ans )
			{
			if ( l_list )				/* The list is on this page: out	*/
				return	l_first;

			l_topic[0] = '\0';			/* Back to the list			*/
			continue;
			}

		snprintf(l_topic, sizeof(l_topic), "%s", l_ans);
		}
}

/*
 *  DESCRIPTION: EXIT and QUIT. EXIT writes every modified buffer that has a file and asks about the others
 *  (name prompted for); QUIT asks once whether to lose the modifications. On the way out the journal is
 *  deleted and the terminal restored, and the messages produced meanwhile (one WRITTEN per file) are printed,
 *  so the user sees what was written. Does not return on success
 *
 *  INPUTS:
 *	a_save		1 for EXIT, 0 for QUIT
 *
 *  IMPLICIT INPUTS:
 *	s_buffers	the registry
 *
 *  RETURNS: TPU$_QUITCANCEL, TPU$_EXITCANCEL or STS$K_ERROR when the user backed out or a write failed;
 *  otherwise the process exits with 0
 */
static int	s_exit_common (
	int	a_save
	)
{
TPU$BUFFER *l_b;
char	l_q[128], l_file[256];
int	l_ans;
unsigned l_mark;

	/* Messages from here on are shown after the screen is given back; an untouched MESSAGES buffer
	 * holds one empty line that the first message replaces rather than follows */
	l_mark = (s_msgbuf->nlines == 1 && !s_msgbuf->lines[0]->len) ? 0 : s_msgbuf->nlines;

	for ( l_b = s_buffers; l_b; l_b = l_b->next )
		{
		if ( (l_b->flags & BUF$M_SYSTEM) || !(l_b->flags & BUF$M_MODIFIED) )
			continue;

		if ( !a_save )
			{
			if ( s_yesno("Buffer modifications will not be saved, continue quitting? [Y/N] ") != 1 )
				return	s_tpu_msg(TPU$_QUITCANCEL);
			break;
			}

		if ( *l_b->file )
			{
			if ( TPU$FAILED(s_buf_write(l_b, l_b->file)) )
				return	STS$K_ERROR;
			continue;
			}

		snprintf(l_q, sizeof(l_q), "Write buffer %s? [Y/N] ", l_b->name);

		if ( (l_ans = s_yesno(l_q)) < 0 )
			return	STS$K_ERROR;

		if ( !l_ans )
			continue;

		if ( TPU$FAILED(s_prompt("File name: ", l_file, sizeof(l_file))) || !*l_file )
			return	s_tpu_msg(TPU$_EXITCANCEL);

		if ( TPU$FAILED(s_buf_write(l_b, l_file)) )
			return	STS$K_ERROR;
		}

	s_journal_close(1);
	s_term_fini();

	for ( unsigned l_i = l_mark; l_i < s_msgbuf->nlines; l_i++ )
		{
		fwrite(s_msgbuf->lines[l_i]->text, 1, s_msgbuf->lines[l_i]->len, stdout);
		fputc('\n', stdout);
		}

	exit(0);
}

/* EXIT: write modified buffers and leave. RETURNS only when the exit was cancelled */
static int	s_cmd_exit (
	char	*a_arg
	)
{
	return	s_exit_common(1);
}
/* QUIT: leave without writing. RETURNS only when the quit was cancelled */
static int	s_cmd_quit (
	char	*a_arg
	)
{
	return	s_exit_common(0);
}

/*
 * The command table. Names are what the user types after DO; help is the one-liner
 * shown by the HELP index.
 */
static const TPU$CMD s_cmds[] = {
	{"BOTTOM",		s_cmd_bottom,		CMD$M_NAV, "Move to the end of the buffer"},
	{"BUFFER",		s_cmd_buffer,		0, "Switch to a buffer, creating it if needed; no name - previous buffer"},
	{"CHANGE CASE",		s_cmd_change_case,	0, "Swap the case of the selection, or of the character under the cursor"},
	{"CHANGE MODE",		s_cmd_change_mode,	0, "Toggle between insert and overstrike"},
	{"DEFINE KEY",		s_cmd_define_key,		0, "Bind a command to a key"},
	{"DELETE BUFFER",	s_cmd_delete_buffer,	0, "Delete a buffer"},
	{"DO",			s_cmd_do,			0, "Prompt for a command"},
	{"END OF LINE",		s_cmd_end_of_line,	CMD$M_NAV, "Move to the end of the current line"},
	{"ERASE CHARACTER",	s_cmd_erase_char,		0, "Erase the character under the cursor"},
	{"ERASE END OF LINE",	s_cmd_erase_eol,		0, "Erase from the cursor to the end of the line"},
	{"ERASE LINE",		s_cmd_erase_line,		0, "Erase the current line"},
	{"ERASE PREVIOUS",	s_cmd_erase_previous,	0, "Erase the character before the cursor"},
	{"ERASE START OF LINE",	s_cmd_erase_sol,		0, "Erase from the start of the line to the cursor"},
	{"ERASE WORD",		s_cmd_erase_word,		0, "Erase the word under the cursor"},
	{"EXIT",		s_cmd_exit,		0, "Write modified buffers and leave the editor"},
	{"FIND",		s_cmd_find,		CMD$M_NAV, "Search for a string in the current direction"},
	{"FIND NEXT",		s_cmd_find_next,		CMD$M_NAV, "Search again for the last string"},
	{"FORWARD",		s_cmd_forward,		CMD$M_NAV, "Set the direction to forward"},
	{"GET FILE",		s_cmd_get_file,		0, "Read a file into a buffer of the same name"},
	{"GOLD",		s_cmd_gold,		CMD$M_NAV, "Prefix key; GOLD digits key repeats the key"},
	{"HELP",		s_cmd_help,		0, "Help on a topic; HELP KEYS lists key bindings"},
	{"HINTS",		s_cmd_hints,		0, "Show or hide the key hint line"},
	{"INCLUDE FILE",	s_cmd_include_file,	0, "Insert a file at the cursor"},
	{"INSERT HERE",		s_cmd_insert_here,	0, "Insert the INSERT HERE buffer at the cursor"},
	{"INSERT MODE",		s_cmd_insert_mode,	0, "Set insert mode"},
	{"LEARN",		s_cmd_learn,		0, "Start recording keys; Ctrl-R to remember"},
	{"LINE",		s_cmd_line,		CMD$M_NAV, "Go to a line by number"},
	{"LOAD KEYS",		s_cmd_load_keys,	0, "Read key definitions from a file"},
	{"MOVE BY CHAR",	s_cmd_move_by_char,	CMD$M_NAV, "Move a character in the current direction"},
	{"MOVE BY LINE",	s_cmd_move_by_line,	CMD$M_NAV, "Move to the end (start) of the line in the current direction"},
	{"MOVE BY LINE START",	s_cmd_move_by_line_start, CMD$M_NAV, "Move to the start of the next (previous) line, as EDT's LINE"},
	{"MOVE BY WORD",	s_cmd_move_by_word,	CMD$M_NAV, "Move a word in the current direction"},
	{"NEXT WORD",		s_cmd_next_word,	CMD$M_NAV, "Move a word forward, whatever the direction"},
	{"MOVE DOWN",		s_cmd_move_down,		CMD$M_NAV, "Move the cursor down"},
	{"MOVE LEFT",		s_cmd_move_left,		CMD$M_NAV, "Move the cursor left"},
	{"MOVE RIGHT",		s_cmd_move_right,		CMD$M_NAV, "Move the cursor right"},
	{"MOVE UP",		s_cmd_move_up,		CMD$M_NAV, "Move the cursor up"},
	{"NEXT SCREEN",		s_cmd_next_screen,	CMD$M_NAV, "Scroll one screen forward"},
	{"ONE WINDOW",		s_cmd_one_window,		0, "Back to a single window"},
	{"OPEN LINE",		s_cmd_open_line,		0, "Break the line at the cursor, the cursor stays before the break"},
	{"OTHER WINDOW",	s_cmd_other_window,	0, "Move the cursor to the other window"},
	{"OVERSTRIKE MODE",	s_cmd_overstrike_mode,	0, "Set overstrike mode"},
	{"PREVIOUS SCREEN",	s_cmd_prev_screen,	CMD$M_NAV, "Scroll one screen back"},
	{"PREVIOUS WORD",	s_cmd_previous_word,	CMD$M_NAV, "Move a word back, whatever the direction"},
	{"QUIT",		s_cmd_quit,		0, "Leave the editor without writing anything"},
	{"REFRESH",		s_cmd_refresh,		CMD$M_NAV, "Re-read the screen size and repaint"},
	{"REMEMBER",		s_cmd_remember,		0, "Stop recording and bind the sequence to a key"},
	{"REMOVE",		s_cmd_remove,		0, "Cut the selection into the INSERT HERE buffer"},
	{"REPEAT",		s_cmd_repeat,		0, "Repeat the next key N times"},
	{"REPLACE",		s_cmd_replace,		0, "Replace a string, asking each time"},
	{"RESET",		s_cmd_reset,		0, "Cancel the selection"},
	{"RESTORE",		s_cmd_restore,		0, "Insert back the last erased word or line"},
	{"RETURN",		s_cmd_return,		0, "Split the line at the cursor"},
	{"SAVE KEYS",		s_cmd_save_keys,	0, "Write the key definitions to a file"},
	{"REVERSE",		s_cmd_reverse,		CMD$M_NAV, "Set the direction to reverse"},
	{"SELECT",		s_cmd_select,		0, "Start (or cancel) a selection at the cursor"},
	{"SHOW BUFFERS",	s_cmd_show_buffers,	0, "List all buffers"},
	{"SHOW KEYS",		s_cmd_show_keys,	0, "List the current key bindings"},
	{"START OF LINE",	s_cmd_start_of_line,	CMD$M_NAV, "Move to the start of the current line"},
	{"STORE TEXT",		s_cmd_store_text,		0, "Copy the selection into the INSERT HERE buffer"},
	{"TAB",			s_cmd_tab,		0, "Insert a TAB character"},
	{"TOP",			s_cmd_top,		CMD$M_NAV, "Move to the start of the buffer"},
	{"TWO WINDOWS",		s_cmd_two_windows,	0, "Split the screen into two windows"},
	{"WILDCARD FIND",	s_cmd_wildcard_find,	CMD$M_NAV, "Find a line matching a pattern with * and %"},
	{"WRITE FILE",		s_cmd_write_file,		0, "Write the buffer to its file, or to a named file"},
};

/* Default bindings: the keypad with the PF row on / * - of a PC (F1 is HELP), EVE control keys, GOLD shortcuts */
static const struct { int key; const char *cmd; } s_defkeys[] = {
	{KEY_PF1, "HELP"},		{KEY_KPSLASH, "GOLD"},		{KEY_KPSTAR, "FIND NEXT"},	{KEY_KPMINUS, "DO"},
	{KEY_KPPLUS, "ERASE CHARACTER"},
	{KEY_KP0, "MOVE BY LINE START"},	{KEY_KP1, "MOVE BY WORD"},	{KEY_KP2, "END OF LINE"},	{KEY_KP3, "MOVE BY CHAR"},
	{KEY_KP4, "FORWARD"},		{KEY_KP5, "REVERSE"},		{KEY_KP6, "REMOVE"},		{KEY_KP7, "DO"},
	{KEY_KP8, "NEXT SCREEN"},	{KEY_KP9, "ERASE LINE"},	{KEY_KPCOMMA, "ERASE CHARACTER"},
	{KEY_KPPERIOD, "SELECT"},	{KEY_KPENTER, "RETURN"},
	{KEY_UP, "MOVE UP"},		{KEY_DOWN, "MOVE DOWN"},	{KEY_LEFT, "MOVE LEFT"},	{KEY_RIGHT, "MOVE RIGHT"},
	{KEY_FIND, "FIND"},		{KEY_INSERT, "INSERT HERE"},	{KEY_REMOVE, "REMOVE"},		{KEY_SELECT, "SELECT"},
	{KEY_PREV, "PREVIOUS SCREEN"},	{KEY_NEXT, "NEXT SCREEN"},	{KEY_HOME, "START OF LINE"},	{KEY_END, "END OF LINE"},
	{0x01, "CHANGE MODE"},		{0x05, "END OF LINE"},		{0x08, "START OF LINE"},	{0x0a, "ERASE WORD"},
	{0x0b, "LEARN"},		{0x12, "REMEMBER"},		{0x15, "ERASE START OF LINE"},	{0x17, "REFRESH"},
	{0x1a, "EXIT"},			{0x7f, "ERASE PREVIOUS"},	{0x0d, "RETURN"},		{0x09, "TAB"},
	{TPU$M_GOLD | KEY_KP0, "OPEN LINE"},	{TPU$M_GOLD | KEY_KP1, "CHANGE CASE"},	{TPU$M_GOLD | KEY_KP2, "ERASE END OF LINE"},
	{TPU$M_GOLD | KEY_KP4, "BOTTOM"},	{TPU$M_GOLD | KEY_KP5, "TOP"},		{TPU$M_GOLD | KEY_KP6, "INSERT HERE"},
	{TPU$M_GOLD | KEY_KP7, "SHOW BUFFERS"},	{TPU$M_GOLD | KEY_KP8, "PREVIOUS SCREEN"}, {TPU$M_GOLD | KEY_KP9, "STORE TEXT"},
	{TPU$M_GOLD | KEY_KPMINUS, "RESTORE"},	{TPU$M_GOLD | KEY_KPCOMMA, "RESTORE"},	{TPU$M_GOLD | KEY_KPPERIOD, "RESET"},
	{TPU$M_GOLD | KEY_KPPLUS, "RESTORE"},	{TPU$M_GOLD | KEY_KPSTAR, "FIND"},
	{TPU$M_GOLD | KEY_FIND, "REPLACE"},	{TPU$M_GOLD | KEY_REMOVE, "STORE TEXT"}, {TPU$M_GOLD | KEY_SELECT, "RESET"},
	{TPU$M_GOLD | KEY_UP, "TOP"},		{TPU$M_GOLD | KEY_DOWN, "BOTTOM"},
	{TPU$M_GOLD | KEY_PF1, "SHOW KEYS"},	{TPU$M_GOLD | 'k', "SHOW KEYS"},
	/* PC keypad in an emulator without application keypad mode: the digits are the cursor block with
	 * NumLock off, 5 is Begin, / * - + are text, Enter is Return: no GOLD on the keypad, Esc is GOLD */
	{0x1b, "GOLD"},				{KEY_BEGIN, "SELECT"},			{TPU$M_GOLD | KEY_BEGIN, "RESET"},
	{TPU$M_GOLD | 0x0d, "DO"},		{TPU$M_GOLD | KEY_KPENTER, "DO"},
	{TPU$M_GOLD | KEY_HOME, "TOP"},		{TPU$M_GOLD | KEY_END, "BOTTOM"},
	{TPU$M_GOLD | KEY_LEFT, "PREVIOUS WORD"}, {TPU$M_GOLD | KEY_RIGHT, "NEXT WORD"},
	{TPU$M_GOLD | '-', "REVERSE"},		{TPU$M_GOLD | '+', "FORWARD"},
	{TPU$M_GOLD | '/', "HELP"},		{TPU$M_GOLD | '*', "SHOW KEYS"},
	{TPU$M_GOLD | KEY_INSERT, "STORE TEXT"},
	{TPU$M_GOLD | 'b', "BUFFER"},		{TPU$M_GOLD | 'd', "DEFINE KEY"},	{TPU$M_GOLD | 'f', "FIND"},
	{TPU$M_GOLD | 'g', "GET FILE"},		{TPU$M_GOLD | 'h', "HELP"},		{TPU$M_GOLD | 'i', "INCLUDE FILE"},
	{TPU$M_GOLD | 'l', "LINE"},		{TPU$M_GOLD | 'q', "QUIT"},		{TPU$M_GOLD | 'r', "REPLACE"},
	{TPU$M_GOLD | 's', "SHOW BUFFERS"},	{TPU$M_GOLD | 't', "TWO WINDOWS"},	{TPU$M_GOLD | 'o', "OTHER WINDOW"},
	{TPU$M_GOLD | 'w', "WRITE FILE"},	{TPU$M_GOLD | 'x', "EXIT"},		{TPU$M_GOLD | 'z', "ONE WINDOW"},
	{TPU$M_GOLD | '?', "HELP"},
};

/*
 *  DESCRIPTION: Load the default key table s_defkeys[]; a lower-case GOLD letter answers to the upper-case
 *  one as well
 *
 *  IMPLICIT INPUTS:
 *	s_defkeys, s_cmds	the tables
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_keydefs, s_nkeydefs	filled
 *
 *  RETURNS: nothing
 */
static void	s_keydef_defaults (
	void
	)
{
const TPU$CMD *l_cmd;

	for ( int l_i = 0; l_i < (int) $ARRSZ(s_defkeys); l_i++ )
		{
		if ( !(l_cmd = s_cmd_by_name(s_defkeys[l_i].cmd)) )
			{
			$LOG(STS$K_ERROR, "default key table refers to unknown command '%s'", s_defkeys[l_i].cmd);
			continue;
			}

		s_keydef_set(s_defkeys[l_i].key, KD$K_CMD, l_cmd, NULL, 0);

		/* Lower-case GOLD letters answer to upper-case as well */
		if ( (s_defkeys[l_i].key & TPU$M_GOLD) && islower(s_defkeys[l_i].key & 0xff) )
			s_keydef_set(TPU$M_GOLD | toupper(s_defkeys[l_i].key & 0xff), KD$K_CMD, l_cmd, NULL, 0);
		}
}

/*
 * =====================================================================================
 *	Main
 * =====================================================================================
 */

/*
 *  DESCRIPTION: Where $LOG and $TRACE go. Their default is the standard output, which is the screen: while
 *  the terminal is ours (raw mode) they are sent to /dev/null, before and after to the standard error. A
 *  debug build sends them to tpu.log all the time
 *
 *  INPUTS:
 *	a_raw		non-zero while the terminal is in raw mode
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_logfd		the descriptor opened on first use
 *
 *  RETURNS: nothing
 */
static void	s_log_redirect (
	int	a_raw
	)
{
#ifdef	_DEBUG
	if ( s_logfd < 0 )
		s_logfd = open("tpu.log", O_WRONLY | O_APPEND | O_CREAT, 0644);

	__util$setlogfd(s_logfd >= 0 ? s_logfd : STDERR_FILENO);
#else
	if ( a_raw && s_logfd < 0 )
		s_logfd = open("/dev/null", O_WRONLY);

	__util$setlogfd((a_raw && s_logfd >= 0) ? s_logfd : STDERR_FILENO);
#endif
}

/*
 *  DESCRIPTION: Print the usage line and leave with exit code 1. Does not return
 *
 *  RETURNS: nothing
 */
static void	s_usage (
	void
	)
{
	$LOG(STS$K_INFO, "%s %s, usage: tpu [-recover] [-nojournal] [-keys file] [file]", __MODULE__, __IDENT__);
	exit(1);
}

int	main (
	int	argc,
	char	**argv
	)
{
int	l_recover = 0, l_nojournal = 0, l_k, l_sts;
const char *l_file = NULL, *l_base, *l_keys = NULL, *l_home;
char	l_path[512];
TPU$BUFFER *l_b;

	s_log_redirect(0);					/* Before the first $LOG/$TRACE		*/

	for ( int l_i = 1; l_i < argc; l_i++ )
		{
		if ( argv[l_i][0] == '-' || argv[l_i][0] == '/' )
			{
			const char *l_o = argv[l_i] + 1;

			if ( !strncasecmp(l_o, "recover", strlen(l_o)) && strlen(l_o) > 1 )
				l_recover = 1;
			else if ( !strncasecmp(l_o, "nojournal", strlen(l_o)) && strlen(l_o) > 1 )
				l_nojournal = 1;
			else if ( !strncasecmp(l_o, "keys", strlen(l_o)) && strlen(l_o) > 1 && l_i + 1 < argc )
				l_keys = argv[++l_i];
			else if ( !strncasecmp(l_o, "version", strlen(l_o)) )
				printf("%s %s\n", __MODULE__, __IDENT__), exit(0);
			else	s_usage();
			}
		else if ( !l_file )
			l_file = argv[l_i];
		else	s_usage();
		}

	setlocale(LC_CTYPE, "");				/* wcwidth() needs the user's locale	*/
	__util$inimsg(&s_msgdesc);				/* Register the message catalogue	*/

	/* System buffers first, so that messages have a place to go */
	s_msgbuf = s_buf_create("MESSAGES", BUF$M_SYSTEM | BUF$M_NOWRITE);
	s_pastebuf = s_buf_create("INSERT HERE", BUF$M_SYSTEM);
	s_restorebuf = s_buf_create("$RESTORE", BUF$M_SYSTEM);
	s_helpbuf = s_buf_create("HELP", BUF$M_SYSTEM | BUF$M_NOWRITE);
	s_buf_set_lines(s_helpbuf, tpu_help_text);

	s_ncmds = $ARRSZ(s_cmds);
	s_keydef_defaults();

	if ( l_file )
		{
		l_base = strrchr(l_file, '/');
		l_b = s_buf_create(l_base ? l_base + 1 : l_file, 0);
		s_buf_load(l_b, l_file);
		snprintf(s_journal_name, sizeof(s_journal_name), "%s.tjl", l_file);
		}
	else	{
		l_b = s_buf_create("MAIN", 0);
		snprintf(s_journal_name, sizeof(s_journal_name), "tpu.tjl");
		}

	/* Key files, after the text so that their complaints stay on the message line */
	if ( (l_home = getenv("HOME")) )
		{
		snprintf(l_path, sizeof(l_path), "%s/%s", l_home, TPU$K_KEYFILE_HOME);
		s_keys_load(l_path, 1);
		}

	s_keys_load(TPU$K_KEYFILE, 1);

	if ( l_keys || (l_keys = getenv("TPU_KEYS")) )
		s_keys_load(l_keys, 0);

	if ( l_recover && TPU$FAILED(s_journal_load()) )
		return	$LOG(STS$K_ERROR, "%s", s_msg), 1;

	if ( TPU$FAILED(s_term_init()) )			/* It has told what is wrong		*/
		return	1;

	if ( !l_nojournal )
		s_journal_open();

	s_win[0].buf = l_b;
	s_scr_getsize();
	s_scr_relayout();

	for (;;)
		{
		s_scr_render();
		l_k = s_key_get();
		s_learn_mark = s_learn_len;
		l_sts = s_dispatch(l_k);

		if ( l_k >= 0xC0 && l_k < 0xF8 )		/* UTF-8 lead byte: take the rest now	*/
			for ( int l_n = (l_k >= 0xF0) ? 3 : (l_k >= 0xE0) ? 2 : 1; l_n > 0 && !TPU$FAILED(l_sts); l_n-- )
				l_sts = s_dispatch(s_key_get());

		if ( TPU$FAILED(l_sts) )
			{
			if ( s_recover_pos < s_recover_len )
				{
				char	l_why[256];

				snprintf(l_why, sizeof(l_why), "%s", s_msg);
				s_tpu_msg(TPU$_RECOVSTOP, s_recover_pos, s_recover_len, l_why);
				}
			s_pb_flush();
			}
		}
}
