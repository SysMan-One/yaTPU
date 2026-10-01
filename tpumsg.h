/*
**++
**
**  FACILITY:  TPU - message catalogue, the .MSG file of the editor
**
**  ABSTRACT:  One X() line per message: symbolic name, severity letter (S, I, W, E, F) and
**	the FAO text. tpu.c turns the list into TPU$_<NAME> condition codes in the VMS layout
**	(facility, message number, severity) and into EMSG_RECORDs for StarLet's __util$getmsg().
**	The text on the message line is "%TPU-<sev>-<NAME>, <text>".
**
**  AUTHORS:   StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  11-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	11-SEP-2026	RRL	X-00-01 (REV 00.01.00) : Initial revision.
**	28-SEP-2026	RRL	X-00-02 (REV 00.02.00) : Header brought to the StarLet coding rules; the ident is
**				taken from tpu.c, the head module of the project.
**	28-SEP-2026	RRL	X-00-05 (REV 00.05.00) : HINTSON, HINTSOFF for the HINTS command.
**	29-SEP-2026	RRL	X-00-09 (REV 00.09.00) : KEYSLOADED, KEYSERRORS, KEYSSAVED, BADKEYLINE, BADKEY, BADKEYCMD,
**				BADMACRO for the key files.
**
**--
*/
#ifndef	__TPUMSG_H__
#define	__TPUMSG_H__	1

#define	TPU$_FACILITY	0x0AB					/* Facility number of the editor	*/

#define	TPU$MESSAGES \
	X(NEWFILE,	I, "%s: new file") \
	X(OPENIN,	E, "cannot open %s: %s") \
	X(READ,		S, "%u lines read from file %s") \
	X(OPENOUT,	E, "cannot create %s: %s") \
	X(WRITEERR,	E, "error writing %s: %s") \
	X(WRITTEN,	S, "%u lines written to file %s") \
	X(READONLY,	E, "buffer %s is read-only") \
	X(NOSELECT,	E, "no select range active") \
	X(EMPTYSEL,	E, "select range is empty") \
	X(NOSEARCH,	E, "no search string") \
	X(NOTFOUND,	E, "could not find: %.*s") \
	X(NOMATCHLINE,	E, "could not find a line matching: %s") \
	X(QUEOVF,	E, "key playback queue overflow, sequence aborted") \
	X(JNLCREATE,	E, "cannot create journal %s: %s") \
	X(JNLOPEN,	E, "cannot open journal %s: %s") \
	X(RECOVERING,	I, "recovering %d keys from %s") \
	X(RECOVSTOP,	E, "recovery stopped at key %d of %d: %s") \
	X(LEARNOVF,	E, "learn sequence too long, learning aborted") \
	X(CANCEL,	E, "cancelled") \
	X(UNKCMD,	E, "unknown command: %s") \
	X(AMBCMD,	E, "ambiguous command: %s") \
	X(KEYUNDEF,	E, "key %s is not defined") \
	X(PRESSRET,	I, "press Return to continue") \
	X(NOPREVBUF,	E, "no previous buffer") \
	X(BUFCREATED,	I, "buffer %s created") \
	X(BUFEXISTS,	I, "buffer %s already exists") \
	X(SYSBUF,	E, "buffer %s is a system buffer") \
	X(NOSUCHBUF,	E, "no such buffer: %s") \
	X(NOTDELETED,	E, "buffer %s not deleted") \
	X(BUFDELETED,	S, "buffer %s deleted") \
	X(BADLINE,	E, "bad line number: %s") \
	X(REPLACED,	I, "%d replacement%s made") \
	X(SELCANCEL,	I, "selection cancelled") \
	X(SELECT,	I, "move the cursor to select text") \
	X(STORED,	I, "text copied to the INSERT HERE buffer") \
	X(NOTHING,	E, "nothing to %s") \
	X(ENDOFBUF,	E, "already at end of buffer") \
	X(TOPOFBUF,	E, "already at start of buffer") \
	X(ONEWINDOW,	I, "already one window") \
	X(TWOWINDOWS,	I, "already two windows") \
	X(ONLYONEWIN,	E, "only one window") \
	X(ALRLEARN,	E, "already learning, press Ctrl-R to remember") \
	X(LEARNING,	I, "learning, press Ctrl-R to remember") \
	X(NOTLEARN,	E, "not learning") \
	X(NOLEARN,	E, "nothing learned") \
	X(DISCARDED,	E, "learned sequence discarded") \
	X(KEYTBLFULL,	E, "key definition table is full") \
	X(KEYLEARNED,	I, "key %s defined as a learned sequence of %d keys") \
	X(BADCOUNT,	E, "bad repeat count: %s") \
	X(KEYDEFINED,	I, "key %s defined as %s") \
	X(NOHELP,	E, "no help on %s") \
	X(QUITCANCEL,	E, "quit cancelled") \
	X(EXITCANCEL,	E, "exit cancelled") \
	X(HINTSON,	I, "key hint line on") \
	X(HINTSOFF,	I, "key hint line off") \
	X(KEYSLOADED,	I, "%d key definitions loaded from %s") \
	X(KEYSERRORS,	E, "%d key definitions loaded from %s, %d bad lines") \
	X(KEYSSAVED,	S, "%d key definitions written to %s") \
	X(BADKEYLINE,	E, "%s line %d: expected keys = command") \
	X(BADKEY,	E, "%s line %d: unknown key \"%.*s\"") \
	X(BADKEYCMD,	E, "%s line %d: unknown command \"%s\"") \
	X(BADMACRO,	E, "%s line %d: bad KEYS string") \
	X(NOMSG,	F, "message number %#x is not in the catalogue")

/* Severity letters to STS$K_ codes */
#define	TPU$SEV_S	STS$K_SUCCESS
#define	TPU$SEV_I	STS$K_INFO
#define	TPU$SEV_W	STS$K_WARN
#define	TPU$SEV_E	STS$K_ERROR
#define	TPU$SEV_F	STS$K_FATAL

/* Message numbers 1..N in list order, then the condition codes: | facility | number | severity | */
enum	{
	TPU$I__FIRST = 0,
#define	X(n, l, t)	TPU$I_##n,
	TPU$MESSAGES
#undef	X
	TPU$I__LAST
};

#define	TPU$CODE(n, sev)	($DEFFAC(TPU$_FACILITY) | $DEFMSG(n) | $DEFSEV(sev))

enum	{
#define	X(n, l, t)	TPU$_##n = TPU$CODE(TPU$I_##n, TPU$SEV_##l),
	TPU$MESSAGES
#undef	X
};

#endif	/* __TPUMSG_H__ */
