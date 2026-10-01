#define	__MODULE__	"KEYCODE"
#define	__IDENT__	"X-00-06"
#define	__REV__		"00.06.00"

/*
**++
**
**  FACILITY:  TPU - keycode, a small utility that shows what the terminal sends for a key
**
**  ABSTRACT:  Press a key: the bytes arrive in raw mode, are printed in hex and in a readable form
**	(ESC, ^X, the character itself) together with the name TPU gives the key, decoded through the same
**	table as the editor (tpukeys.h). A sequence TPU does not know is reported as such: that is the
**	answer to "why does this key do nothing in TPU", and the sequence to add to s_keyseqs[].
**
**	The keypad and the cursor keys are put into application mode, exactly as the editor does,
**	unless -normal is given. Ctrl-C leaves.
**
**  AUTHORS:   StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  28-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	28-SEP-2026	RRL	X-00-06 (REV 00.06.00) : Initial revision.
**
**--
*/

#define	__FAC__		"KEYCODE"
#define	__TFAC__	__FAC__ ": "
#include	"utility_routines.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>
#include	<unistd.h>
#include	<errno.h>
#include	<termios.h>
#include	<poll.h>

#include	"tpukeys.h"

#define	KEYCODE$K_SEQSZ		16				/* Longest sequence we collect		*/
#define	KEYCODE$K_SEQGAP	80				/* ms between bytes of one sequence	*/

static struct termios	s_tio_saved;
static int		s_term_raw;
static int		s_appmode = 1;				/* Keypad/cursor keys in application mode	*/

/*
 *  DESCRIPTION: Read one byte from the terminal, with an optional timeout
 *
 *  INPUTS:
 *	a_timeout_ms	milliseconds to wait, -1 to block
 *
 *  RETURNS: the byte, or -1 on timeout, interrupt or end of file
 */
static int	s_getc (
	int	a_timeout_ms
	)
{
unsigned char	l_c;
struct pollfd	l_pfd = {STDIN_FILENO, POLLIN, 0};

	if ( a_timeout_ms >= 0 && poll(&l_pfd, 1, a_timeout_ms) <= 0 )
		return	-1;

	return	(read(STDIN_FILENO, &l_c, 1) == 1) ? l_c : -1;
}

/*
 *  DESCRIPTION: Give the terminal back: keypad and cursor keys to normal mode, saved settings restored.
 *  Safe to call more than once
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

	if ( s_appmode )
		write(STDOUT_FILENO, "\033[?1l\033>", 7);

	tcsetattr(STDIN_FILENO, TCSANOW, &s_tio_saved);
	s_term_raw = 0;
}

/*
 *  DESCRIPTION: Put the terminal into raw mode and, unless -normal, the keypad and cursor keys into
 *  application mode, as the editor does
 *
 *  IMPLICIT INPUTS/OUTPUTS:
 *	s_tio_saved	the original terminal settings; s_term_raw - set
 *
 *  RETURNS: STS$K_SUCCESS, or STS$K_ERROR when standard input is not a terminal or the mode cannot be set
 */
static int	s_term_init (
	void
	)
{
struct termios	l_tio;

	if ( !isatty(STDIN_FILENO) )
		return	$LOG(STS$K_ERROR, "standard input is not a terminal"), STS$K_ERROR;

	if ( tcgetattr(STDIN_FILENO, &s_tio_saved) )
		return	$LOG(STS$K_ERROR, "tcgetattr(): errno=%d, %s", errno, strerror(errno)), STS$K_ERROR;

	l_tio = s_tio_saved;
	cfmakeraw(&l_tio);
	l_tio.c_oflag = s_tio_saved.c_oflag;			/* Keep output cooked: we print lines	*/
	l_tio.c_cc[VMIN] = 1;
	l_tio.c_cc[VTIME] = 0;

	if ( tcsetattr(STDIN_FILENO, TCSANOW, &l_tio) )
		return	$LOG(STS$K_ERROR, "tcsetattr(): errno=%d, %s", errno, strerror(errno)), STS$K_ERROR;

	s_term_raw = 1;

	if ( s_appmode )
		write(STDOUT_FILENO, "\033[?1h\033=", 7);

	return	STS$K_SUCCESS;
}

/*
 *  DESCRIPTION: Name TPU gives a single byte: Delete, Space, Return, Tab, Escape, Ctrl-X, the character, or
 *  "byte" for anything above 127 (a piece of a UTF-8 character)
 *
 *  INPUTS:
 *	a_c		the byte; a_sz - size of a_buf
 *
 *  OUTPUTS:
 *	a_buf		the name
 *
 *  RETURNS: a_buf
 */
static const char *	s_byte_name (
	int	a_c,
	char	*a_buf,
	size_t	a_sz
	)
{
	if ( a_c == 127 )
		snprintf(a_buf, a_sz, "Delete");
	else if ( a_c == 32 )
		snprintf(a_buf, a_sz, "Space");
	else if ( a_c == 13 )
		snprintf(a_buf, a_sz, "Return");
	else if ( a_c == 9 )
		snprintf(a_buf, a_sz, "Tab");
	else if ( a_c == 27 )
		snprintf(a_buf, a_sz, "Escape");
	else if ( a_c < 32 )
		snprintf(a_buf, a_sz, "Ctrl-%c", a_c + '@');
	else if ( a_c < 128 )
		snprintf(a_buf, a_sz, "%c", a_c);
	else	snprintf(a_buf, a_sz, "byte");

	return	a_buf;
}

/*
 *  DESCRIPTION: Print one key: the bytes in hex, a readable rendering and the TPU name
 *
 *  INPUTS:
 *	a_seq		the bytes; a_n - their count
 *
 *  RETURNS: nothing
 */
static void	s_report (
	const unsigned char	*a_seq,
	int			a_n
	)
{
char	l_hex[KEYCODE$K_SEQSZ * 3 + 1], l_text[KEYCODE$K_SEQSZ * 4 + 1], l_name[32];
int	l_h = 0, l_t = 0;
const char *l_what;

	for ( int l_i = 0; l_i < a_n; l_i++ )
		{
		l_h += snprintf(l_hex + l_h, sizeof(l_hex) - l_h, "%02x ", a_seq[l_i]);

		if ( a_seq[l_i] == 27 )
			l_t += snprintf(l_text + l_t, sizeof(l_text) - l_t, "ESC ");
		else if ( a_seq[l_i] < 32 )
			l_t += snprintf(l_text + l_t, sizeof(l_text) - l_t, "^%c ", a_seq[l_i] + '@');
		else if ( a_seq[l_i] == 127 )
			l_t += snprintf(l_text + l_t, sizeof(l_text) - l_t, "DEL ");
		else if ( a_seq[l_i] < 128 )
			l_t += snprintf(l_text + l_t, sizeof(l_text) - l_t, "%c ", a_seq[l_i]);
		else	l_text[l_t++] = a_seq[l_i];		/* UTF-8 bytes: the character comes out whole	*/
		}

	l_text[l_t] = '\0';

	if ( a_n == 1 )
		l_what = s_byte_name(a_seq[0], l_name, sizeof(l_name));
	else if ( a_seq[0] >= 0xC0 )
		l_what = "UTF-8 character";
	else	{
		l_what = "unknown to TPU";

		for ( int l_i = 0; l_i < (int) $ARRSZ(s_keyseqs); l_i++ )
			if ( (int) strlen(s_keyseqs[l_i].seq) == a_n && !memcmp(s_keyseqs[l_i].seq, a_seq, a_n) )
				{
				l_what = s_keynames[s_keyseqs[l_i].key - TPU$K_KEYBASE];
				break;
				}
		}

	printf("%-24s %-20s %s\n", l_hex, l_text, l_what);
	fflush(stdout);
}

int	main (
	int	argc,
	char	**argv
	)
{
unsigned char	l_seq[KEYCODE$K_SEQSZ];
int	l_c, l_n;

	__util$setlogfd(STDERR_FILENO);

	for ( int l_i = 1; l_i < argc; l_i++ )
		{
		const char *l_o = argv[l_i] + 1;

		if ( (argv[l_i][0] == '-' || argv[l_i][0] == '/') && !strncasecmp(l_o, "normal", strlen(l_o)) && strlen(l_o) > 1 )
			s_appmode = 0;
		else if ( (argv[l_i][0] == '-' || argv[l_i][0] == '/') && !strncasecmp(l_o, "version", strlen(l_o)) )
			printf("%s %s\n", __MODULE__, __IDENT__), exit(0);
		else	{
			$LOG(STS$K_INFO, "%s %s, usage: keycode [-normal]   (-normal: keypad and cursor keys left in numeric mode)", __MODULE__, __IDENT__);
			exit(1);
			}
		}

	if ( !(1 & s_term_init()) )
		return	1;

	printf("%s %s: press keys, Ctrl-C to leave. Keypad in %s mode.\n", __MODULE__, __IDENT__, s_appmode ? "application" : "numeric");
	printf("%-24s %-20s %s\n", "bytes", "as text", "TPU key");
	fflush(stdout);

	for (;;)
		{
		if ( (l_c = s_getc(-1)) < 0 )
			break;

		l_n = 0;
		l_seq[l_n++] = l_c;

		/* An escape sequence or a UTF-8 character: the rest follows at once */
		if ( l_c == 27 || l_c >= 0xC0 )
			while ( l_n < KEYCODE$K_SEQSZ && (l_c = s_getc(KEYCODE$K_SEQGAP)) >= 0 )
				l_seq[l_n++] = l_c;

		s_report(l_seq, l_n);

		if ( l_n == 1 && l_seq[0] == 3 )		/* Ctrl-C				*/
			break;
		}

	s_term_fini();

	return	0;
}
