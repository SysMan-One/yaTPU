/*
**++
**
**  FACILITY:  TPU - key codes and the terminal escape sequences that produce them
**
**  ABSTRACT:  Shared by the editor (tpu.c) and the keycode utility (keycode.c). Bytes 0-255 are
**	themselves, special keys start at TPU$K_KEYBASE, the GOLD prefix is a flag OR-ed to the
**	following key. s_keyseqs[] maps the escape sequences of a VT220 and of xterm to the keys;
**	both flavours are just rows of the same table.
**
**  AUTHORS:   StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  28-SEP-2026
**
**  MODIFICATION HISTORY:
**
**	28-SEP-2026	RRL	X-00-06 (REV 00.06.00) : Initial revision, moved out of tpu.c for keycode.c.
**	29-SEP-2026	RRL	X-00-07 (REV 00.07.00) : Begin (ESC [ E, ESC O E): the 5 of a PC keypad with NumLock off.
**
**--
*/
#ifndef	__TPUKEYS_H__
#define	__TPUKEYS_H__	1

#define	TPU$K_KEYBASE	0x10000
#define	TPU$M_GOLD	0x20000
#define	KEY_NONE	(-1)

#define	TPU$KEYS \
	X(CPR, "CPR")		X(UP, "Up")		X(DOWN, "Down")		X(LEFT, "Left")		X(RIGHT, "Right")	\
	X(PF1, "PF1")		X(PF2, "PF2")		X(PF3, "PF3")		X(PF4, "PF4")					\
	X(KP0, "KP0")		X(KP1, "KP1")		X(KP2, "KP2")		X(KP3, "KP3")		X(KP4, "KP4")		\
	X(KP5, "KP5")		X(KP6, "KP6")		X(KP7, "KP7")		X(KP8, "KP8")		X(KP9, "KP9")		\
	X(KPMINUS, "KP-")	X(KPCOMMA, "KP,")	X(KPPERIOD, "KP.")	X(KPENTER, "Enter")				\
	X(FIND, "Find")		X(INSERT, "Insert Here") X(REMOVE, "Remove")	X(SELECT, "Select")				\
	X(PREV, "Prev Screen")	X(NEXT, "Next Screen")	X(HOME, "Home")		X(END, "End")		X(BEGIN, "Begin")	\
	X(F6, "F6")		X(F7, "F7")		X(F8, "F8")		X(F9, "F9")		X(F10, "F10")		\
	X(F11, "F11")		X(F12, "F12")		X(F13, "F13")		X(F14, "F14")		X(F15, "Help")		\
	X(F16, "Do")		X(F17, "F17")		X(F18, "F18")		X(F19, "F19")		X(F20, "F20")

enum	{
	KEY__BASE = TPU$K_KEYBASE - 1,
#define	X(n, s)	KEY_##n,
	TPU$KEYS
#undef	X
	KEY_MAX
};

static const char *s_keynames[] = {
#define	X(n, s)	s,
	TPU$KEYS
#undef	X
};

/* Escape sequence -> key */
typedef	struct	keyseq_t {
	const char *	seq;
	int		key;
} TPU$KEYSEQ;

static const TPU$KEYSEQ s_keyseqs[] = {
	{"\033OA", KEY_UP},	{"\033[A", KEY_UP},	{"\033OB", KEY_DOWN},	{"\033[B", KEY_DOWN},
	{"\033OC", KEY_RIGHT},	{"\033[C", KEY_RIGHT},	{"\033OD", KEY_LEFT},	{"\033[D", KEY_LEFT},
	{"\033OP", KEY_PF1},	{"\033OQ", KEY_PF2},	{"\033OR", KEY_PF3},	{"\033OS", KEY_PF4},
	{"\033[11~", KEY_PF1},	{"\033[12~", KEY_PF2},	{"\033[13~", KEY_PF3},	{"\033[14~", KEY_PF4},
	{"\033Op", KEY_KP0},	{"\033Oq", KEY_KP1},	{"\033Or", KEY_KP2},	{"\033Os", KEY_KP3},
	{"\033Ot", KEY_KP4},	{"\033Ou", KEY_KP5},	{"\033Ov", KEY_KP6},	{"\033Ow", KEY_KP7},
	{"\033Ox", KEY_KP8},	{"\033Oy", KEY_KP9},	{"\033Om", KEY_KPMINUS}, {"\033Ol", KEY_KPCOMMA},
	{"\033On", KEY_KPPERIOD}, {"\033OM", KEY_KPENTER},
	{"\033[1~", KEY_FIND},	{"\033[2~", KEY_INSERT}, {"\033[3~", KEY_REMOVE}, {"\033[4~", KEY_SELECT},
	{"\033[5~", KEY_PREV},	{"\033[6~", KEY_NEXT},
	{"\033[H", KEY_HOME},	{"\033OH", KEY_HOME},	{"\033[F", KEY_END},	{"\033OF", KEY_END},
	{"\033[E", KEY_BEGIN},	{"\033OE", KEY_BEGIN},						/* PC keypad 5 with NumLock off	*/
	{"\033[17~", KEY_F6},	{"\033[18~", KEY_F7},	{"\033[19~", KEY_F8},	{"\033[20~", KEY_F9},
	{"\033[21~", KEY_F10},	{"\033[23~", KEY_F11},	{"\033[24~", KEY_F12},	{"\033[25~", KEY_F13},
	{"\033[26~", KEY_F14},	{"\033[28~", KEY_F15},	{"\033[29~", KEY_F16},	{"\033[31~", KEY_F17},
	{"\033[32~", KEY_F18},	{"\033[33~", KEY_F19},	{"\033[34~", KEY_F20},
};

#endif	/* __TPUKEYS_H__ */
