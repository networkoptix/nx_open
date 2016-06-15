/****************************************************************************/
/*                                                                          */
/* Copyright 1992 Simmule Turner and Rich Salz.  All rights reserved.       */
/*                                                                          */
/* This software is not subject to any license of the American Telephone    */
/* and Telegraph Company or of the Regents of the University of California. */
/*                                                                          */
/* Permission is granted to anyone to use this software for any purpose on  */
/* any computer system, and to alter it and redistribute it freely, subject */
/* to the following restrictions:                                           */
/* 1. The authors are not responsible for the consequences of use of this   */
/*    software, no matter how awful, even if they arise from flaws in it.   */
/* 2. The origin of this software must not be misrepresented, either by     */
/*    explicit claim or by omission.  Since few users ever read sources,    */
/*    credits must appear in the documentation.                             */
/* 3. Altered versions must be plainly marked as such, and must not be      */
/*    misrepresented as being the original software.  Since few users       */
/*    ever read sources, credits must appear in the documentation.          */
/* 4. This notice may not be removed or altered.                            */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*  This is a line-editing library, it can be linked into almost any        */
/*  program to provide command-line editing and recall.                     */
/*                                                                          */
/*  Posted to comp.sources.misc Sun, 2 Aug 1992 03:05:27 GMT                */
/*      by rsalz@osf.org (Rich $alz)                                        */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*  The version contained here has some modifications by awb@cstr.ed.ac.uk  */
/*  (Alan W Black) in order to integrate it with the Edinburgh Speech Tools */
/*  library and Scheme-in-one-defun in particular, though these changes     */
/*  have a much more general use that just us.  All modifications to        */
/*  to this work are continued with the same copyright above.  That is      */
/*  this version of editline does not have the the "no commercial use"      */
/*  restriction that some of the rest of the EST library may have           */
/*  awb Dec 30 1998                                                         */
/*                                                                          */
/*  Specific additions (there are other smaller ones too, all marked):      */
/*    some ansificiation and prototypes added                               */
/*    storage and retrieval of history over sessions                        */
/*    user definable history completion                                     */
/*    possibles listing in completion                                       */
/*    reverse incremental search                                            */
/*    lines longer than window width (mostly)                               */
/*    reasonable support for 8 bit chars in languages other than English    */
/*                                                                          */
/****************************************************************************/

/*  $Revision: 1.6 $
**
**  Main editing routines for editline library.
*/
#include "editline.h"
#include "EST_unix.h"
#include <ctype.h>

/*
**  Manifest constants.
*/
#define SCREEN_WIDTH	80
#define SCREEN_ROWS	24
#define NO_ARG		(-1)
#define DEL		127
#define ESC		0x1b
#define CTL(x)		(char)((x) & 0x1F)
#define ISCTL(x)	((x) && (x) < ' ')
#define UNCTL(x)	(char)((x) + 64)
#define META(x)		(char)((x) | 0x80)
#define ISMETA(x)	((x) & 0x80)
#define UNMETA(x)	(char)((x) & 0x7F)
/* modified by awb to allow specifcation of history size at run time  */
/* (though only once)                                                 */
int editline_histsize=256;
char *editline_history_file;
/* If this is defined it'll be called for completion first, before the */
/* internal file name completion will be                               */
EL_USER_COMPLETION_FUNCTION_TYPE*el_user_completion_function = NULL;

/*
**  The type of case-changing to perform.
*/
typedef enum _CASE {
    TOupper, TOlower, TOcapitalize
} CASE;

/*
**  Key to command mapping.
*/
typedef struct _KEYMAP {
    ECHAR	Key;
    STATUS	(*Function)();
} KEYMAP;

/*
**  Command history structure.
*/
typedef struct _HISTORY {
    int		Size;
    int		Pos;
    ECHAR	**Lines;
} HISTORY;

/*
**  Globals.
*/
int		rl_eof;
int		rl_erase;
int		rl_intr;
int		rl_kill;

ECHAR		el_NIL[] = "";
extern CONST ECHAR	*el_Input;
STATIC ECHAR		*Line = NULL;
STATIC CONST char	*Prompt = NULL;
STATIC ECHAR		*Yanked = NULL;
STATIC char		*Screen = NULL;
/* STATIC char		NEWLINE[]= CRLF; */
STATIC HISTORY		H;
int		rl_quit;
STATIC int		Repeat;
STATIC int		End;
STATIC int		Mark;
STATIC int		OldPoint;
STATIC int		Point;
extern int		el_PushBack;
extern int		el_Pushed;
FORWARD KEYMAP		Map[33];
FORWARD KEYMAP		MetaMap[64];
STATIC ESIZE_T		Length;
STATIC ESIZE_T		ScreenCount;
STATIC ESIZE_T		ScreenSize;
STATIC ECHAR		*backspace = NULL;
STATIC ECHAR		*upline = NULL;
STATIC ECHAR		*clrpage = NULL;
STATIC ECHAR		*downline = NULL;
STATIC ECHAR		*move_right = NULL;
STATIC ECHAR             *newline = NULL;
STATIC ECHAR             *bol = NULL;
STATIC ECHAR             *nextline = NULL;
STATIC int		TTYwidth;
STATIC int		TTYrows;
STATIC int              RequireNLforWrap = 1;
STATIC int              el_intr_pending = 0;
int                     el_no_echo = 0;  /* e.g under emacs */

/* A little ansification with prototypes -- awb */
extern void TTYflush();
STATIC void TTYput(ECHAR c);
STATIC void TTYputs(ECHAR *p);
STATIC void TTYshow(ECHAR c);
STATIC void TTYstring(ECHAR *p);
extern unsigned int TTYget();
STATIC void TTYinfo();
STATIC void print_columns(int ac, char **av);
STATIC void reposition(int reset);
STATIC void left(STATUS Change);
STATIC void right(STATUS Change);
STATIC STATUS ring_bell();
#if 0
STATIC STATUS do_macro(unsigned int c);
#endif
STATIC STATUS do_forward(STATUS move);
STATIC STATUS do_case(ECHAR type);
STATIC STATUS case_down_word();
STATIC STATUS case_up_word();
STATIC void ceol();
STATIC void clear_line();
STATIC STATUS insert_string(ECHAR *p);
STATIC ECHAR *next_hist();
STATIC ECHAR *prev_hist();
STATIC STATUS do_insert_hist(ECHAR *p);
STATIC STATUS do_hist(ECHAR *(*move)());
STATIC STATUS h_next();
STATIC STATUS h_prev();
STATIC STATUS h_first();
STATIC STATUS h_last();
STATIC int substrcmp(char *text, char *pat, int len);
STATIC ECHAR *search_hist(ECHAR *search, ECHAR *(*move)());
STATIC STATUS h_search();
STATIC STATUS fd_char();
STATIC void save_yank(int begin, int i);
STATIC STATUS delete_string(int count);
STATIC STATUS bk_char();
STATIC STATUS bk_del_char();
STATIC STATUS redisplay();
STATIC STATUS kill_line();
STATIC char *rsearch_hist(char *patt, int *lpos,int *cpos);
STATIC STATUS h_risearch();
STATIC STATUS insert_char(int c);
STATIC STATUS meta();
STATIC STATUS emacs(unsigned int c);
STATIC STATUS TTYspecial(unsigned int c);
STATIC ECHAR *editinput();
STATIC void hist_add(ECHAR *p);
STATIC STATUS beg_line();
STATIC STATUS del_char();
STATIC STATUS end_line();
STATIC ECHAR *find_word();
STATIC STATUS c_complete();
STATIC STATUS c_possible();
STATIC STATUS accept_line();
STATIC STATUS transpose();
STATIC STATUS quote();
STATIC STATUS wipe();
STATIC STATUS mk_set();
STATIC STATUS exchange();
STATIC STATUS yank();
STATIC STATUS copy_region();
STATIC STATUS move_to_char();
STATIC STATUS fd_word();
STATIC STATUS fd_kill_word();
STATIC STATUS bk_word();
STATIC STATUS bk_kill_word();
STATIC int argify(ECHAR *line, ECHAR ***avp);
STATIC STATUS last_argument();

/* Display print 8-bit chars as `M-x' or as the actual 8-bit char? */
int		rl_meta_chars = 0;

/*
**  Declarations.
*/
STATIC ECHAR	*editinput();
#if	defined(USE_TERMCAP)
extern char	*getenv();
extern char	*tgetstr();
extern int	tgetent();
extern int	tgetnum();
#endif	/* defined(USE_TERMCAP) */

/*
**  TTY input/output functions.
*/

void TTYflush()
{
    if (ScreenCount) {
	if (el_no_echo == 0)
	    (void)write(1, Screen, ScreenCount);
	ScreenCount = 0;
    }
}

STATIC void TTYput(ECHAR c)
{
    Screen[ScreenCount] = c;
    if (++ScreenCount >= ScreenSize - 1) {
	ScreenSize += SCREEN_INC;
	RENEW(Screen, char, ScreenSize);
    }
}

STATIC void TTYputs(ECHAR *p)
{
    while (*p)
	TTYput(*p++);
}

STATIC void TTYshow(ECHAR c)
{
    if (c == DEL) {
	TTYput('^');
	TTYput('?');
    }
    else if (ISCTL(c)) {
	TTYput('^');
	TTYput(UNCTL(c));
    }
    else if (rl_meta_chars && ISMETA(c)) {
	TTYput('M');
	TTYput('-');
	TTYput(UNMETA(c));
    }
    else
	TTYput(c);
}

STATIC void TTYstring(ECHAR *p)
{
    while (*p)
	TTYshow(*p++);
}

#if 0
/* Old one line version */
#define TTYback()	(backspace ? TTYputs((ECHAR *)backspace) : TTYput('\b'))
#endif

STATIC int printlen(CONST char *p)
{
    int len = 0;

    for (len=0; *p; p++)
	if ((*p == DEL) || (ISCTL(*p)))
	    len += 2;
	else if (rl_meta_chars && ISMETA(*p))
	    len += 3;
	else
	    len += 1;

    return len;
}

STATIC int screen_pos()
{
    /* Returns the number of characters printed from beginning of line  */
    /* includes the size of the prompt and and meta/ctl char expansions */
    int p = strlen(Prompt);
    int i;
    
    for (i=0; i < Point; i++)
	if ((Line[i] == DEL) ||
	    (ISCTL(Line[i])))
	    p += 2;
	else if (rl_meta_chars && ISMETA(Line[i]))
	    p += 3;
	else
	    p += 1;

    return p;
}

STATIC void TTYback()
{
    /* awb: added upline (if supported) when back goes over line boundary */
    int i;
    int sp = screen_pos();

    if (upline && sp && (sp%TTYwidth == 0))
    {   /* move up a line and move to the end */
	TTYputs(upline);
	TTYputs(bol);
	for (i=0; i < TTYwidth; i++)
	    TTYputs(move_right);
    }
    else if (backspace)
	TTYputs((ECHAR *)backspace);
    else
	TTYput('\b');
}

STATIC void TTYinfo()
{
    static int		init;
#if	defined(USE_TERMCAP)
    char		*term;
    char		*buff;
    char		*buff2;
    char		*bp;
#endif	/* defined(USE_TERMCAP) */
#if	defined(TIOCGWINSZ)
    struct winsize	W;
#endif	/* defined(TIOCGWINSZ) */

    if (init) {
#if	defined(TIOCGWINSZ)
	/* Perhaps we got resized. */
	if (ioctl(0, TIOCGWINSZ, &W) >= 0
	 && W.ws_col > 0 && W.ws_row > 0) {
	    TTYwidth = (int)W.ws_col;
	    TTYrows = (int)W.ws_row;
	}
#endif	/* defined(TIOCGWINSZ) */
	return;
    }
    init++;

    TTYwidth = TTYrows = 0;
#if	defined(USE_TERMCAP)
    buff = walloc(char,2048);
    buff2 = walloc(char,2048);
    bp = &buff2[0];
    if ((term = getenv("TERM")) == NULL)
	term = "dumb";
    if (tgetent(buff, term) < 0) {
       TTYwidth = SCREEN_WIDTH;
       TTYrows = SCREEN_ROWS;
       return;
    }
    backspace = (ECHAR *)tgetstr("le", &bp);
    upline = (ECHAR *)tgetstr("up", &bp);
    clrpage = (ECHAR *)tgetstr("cl", &bp);
    nextline = (ECHAR *)tgetstr("nl", &bp);
    if (nextline==NULL)
      nextline = (ECHAR *)"\n";
    if (strncmp(term, "pcansi", 6)==0 || strncmp(term, "cygwin", 6)==0)
    {
	bol = (ECHAR *)"\033[0G";
	RequireNLforWrap = 0; /* doesn't require nl to get to next line */
    }
    else
	bol = (ECHAR *)tgetstr("cr", &bp);
    if (bol==NULL)
      bol = (ECHAR *)"\r";

    newline= walloc(ECHAR, 20);
    strcpy((char *)newline,(char *)bol);
    strcat((char *)newline,(char *)nextline);

    downline = (ECHAR *)newline;
    move_right = (ECHAR *)tgetstr("nd", &bp);
    if (!move_right || !downline) 
	upline = NULL;  /* terminal doesn't support enough so fall back */
    TTYwidth = tgetnum("co");
    TTYrows = tgetnum("li");
#endif	/* defined(USE_TERMCAP) */

#if	defined(TIOCGWINSZ)
    if (ioctl(0, TIOCGWINSZ, &W) >= 0) {
	TTYwidth = (int)W.ws_col;
	TTYrows = (int)W.ws_row;
    }
#endif	/* defined(TIOCGWINSZ) */

    if (TTYwidth <= 0 || TTYrows <= 0) {
	TTYwidth = SCREEN_WIDTH;
	TTYrows = SCREEN_ROWS;
    }
}


/*
**  Print an array of words in columns.
*/
STATIC void print_columns(int ac, char **av)
{
    ECHAR	*p;
    int		i,c;
    int		j;
    int		k;
    int		len;
    int		skip;
    int		longest;
    int		cols;
    char info1[1024];

    if (ac > 99)
    {
	TTYputs((ECHAR *)newline);
	sprintf(info1,"There are %d possibilities.  Do you really \n",ac);
	TTYputs((ECHAR *)info1);
	TTYputs((ECHAR *)"want to see them all (y/n) ? ");
	while (((c = TTYget()) != EOF) && ((strchr("YyNn ",c) == NULL)))
	    ring_bell();
	if (strchr("Nn",c) != NULL)
	{
	    TTYputs((ECHAR *)newline);
	    return;
	}
    }

    /* Find longest name, determine column count from that. */
    for (longest = 0, i = 0; i < ac; i++)
	if ((j = strlen((char *)av[i])) > longest)
	    longest = j;
    cols = TTYwidth / (longest + 3);
    if (cols < 1) cols = 1;

    TTYputs((ECHAR *)newline);
    for (skip = ac / cols + 1, i = 0; i < skip; i++) {
	for (j = i; j < ac; j += skip) {
	    for (p = (ECHAR *)av[j], len = strlen((char *)p), k = len; 
		 --k >= 0; p++)
		TTYput(*p);
	    if (j + skip < ac)
		while (++len < longest + 3)
		    TTYput(' ');
	}
	TTYputs((ECHAR *)newline);
    }
}

STATIC void reposition(int reset)
{
    int		i,PPoint;
    int pos;
    char ppp[2];

    if (reset)
    {
	TTYputs(bol);
	for (i=screen_pos()/TTYwidth; i > 0; i--)
	    if (upline) TTYputs(upline);
    }
    TTYputs((ECHAR *)Prompt);
    pos = printlen(Prompt);
    ppp[1] = '\0';
    for (i = 0; i < End; i++)
    {
	ppp[0] = Line[i];
	TTYshow(Line[i]);
	pos += printlen(ppp);
	if ((pos%TTYwidth) == 0)
	    if (RequireNLforWrap && downline) TTYputs(downline);
    }
    PPoint = Point;
    for (Point = End; 
	 Point > PPoint; 
	 Point--)
    {
	if (rl_meta_chars && ISMETA(Line[Point]))
	{
	    TTYback();
	    TTYback();
	}
	else if (ISCTL(Line[Point]))
	    TTYback();
	TTYback();
    }
    Point = PPoint;
}

STATIC void left(STATUS Change)
{
    TTYback();
    if (Point) {
	if (ISCTL(Line[Point - 1]))
	    TTYback();
        else if (rl_meta_chars && ISMETA(Line[Point - 1])) {
	    TTYback();
	    TTYback();
	}
    }
    if (Change == CSmove)
	Point--;
}

STATIC void right(STATUS Change)
{
    TTYshow(Line[Point]);
    if (Change == CSmove)
	Point++;
    if ((screen_pos())%TTYwidth == 0)
	if (downline && RequireNLforWrap) TTYputs(downline);    
}

STATIC STATUS ring_bell()
{
    TTYput('\07');
    TTYflush();
    return CSstay;
}

#if 0 
STATIC STATUS do_macro(unsigned int c)
{
    ECHAR		name[4];

    name[0] = '_';
    name[1] = c;
    name[2] = '_';
    name[3] = '\0';

    if ((el_Input = (ECHAR *)getenv((char *)name)) == NULL) {
	el_Input = el_NIL;
	return ring_bell();
    }
    return CSstay;
}
#endif

STATIC STATUS do_forward(STATUS move)
{
    int		i;
    ECHAR	*p;
    (void) move;

    i = 0;
    do {
	p = &Line[Point];
	for ( ; Point < End && (*p == ' ' || !isalnum(*p)); p++)
	    right(CSmove);

	for (; Point < End && isalnum(*p); p++)
	    right(CSmove);

	if (Point == End)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS do_case(ECHAR type)
{
    int		i;
    int		end;
    int		count;
    ECHAR	*p;
    int OP;

    OP = Point;
    (void)do_forward(CSstay);
    if (OP != Point) {
	if ((count = Point - OP) < 0)
	    count = -count;
	for ( ; Point > OP; Point --)
	    TTYback();
	if ((end = Point + count) > End)
	    end = End;
	for (i = Point, p = &Line[Point]; Point < end; p++) {
	    if ((type == TOupper) ||
		((type == TOcapitalize) && (Point == i)))
	    {
		if (islower(*p))
		    *p = toupper(*p);
	    }
	    else if (isupper(*p))
		*p = tolower(*p);
	    right(CSmove);
	}
    }
    return CSstay;
}

STATIC STATUS case_down_word()
{
    return do_case(TOlower);
}

STATIC STATUS case_up_word()
{
    return do_case(TOupper);
}

STATIC STATUS case_cap_word()
{
    return do_case(TOcapitalize);
}

STATIC void ceol()
{
    int		extras;
    int		i, PPoint;
    ECHAR	*p;

    PPoint = Point;
    for (extras = 0, i = Point, p = &Line[i]; i < End; i++, p++) {
	Point++;
	TTYput(' ');
	if (ISCTL(*p)) {
	    TTYput(' ');
	    extras++;
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    TTYput(' ');
	    TTYput(' ');
	    extras += 2;
	}
	else if ((screen_pos())%TTYwidth == 0)
	    if (downline && RequireNLforWrap) TTYputs(downline);
    }

    Point = End;
    for (Point = End; 
	 Point > PPoint; 
	 Point--)
    {
	if (rl_meta_chars && ISMETA(Line[Point-1]))
	{
	    TTYback();
	    TTYback();
	}
	else if (ISCTL(Line[Point-1]))
	    TTYback();
	TTYback();
    }
    Point = PPoint;

}

STATIC void clear_line()
{
    int i;
    TTYputs(bol);
    for (i=screen_pos()/TTYwidth; i > 0; i--)
	if (upline) TTYputs(upline);
    for (i=0; i < strlen(Prompt); i++)
	TTYput(' ');
    Point = 0;
    ceol();
    TTYputs(bol);
    /* In case the prompt is more than one line long */
    for (i=screen_pos()/TTYwidth; i > 0; i--)
	if (upline) TTYputs(upline);
    Point = 0;
    End = 0;
    Line[0] = '\0';
}

STATIC STATUS insert_string(ECHAR *p)
{
    ESIZE_T	len;
    int		i,pos0,pos1;
    ECHAR	*new;
    ECHAR	*q;

    len = strlen((char *)p);
    if (End + len >= Length) {
	if ((new = NEW(ECHAR, Length + len + MEM_INC)) == NULL)
	    return CSstay;
	if (Length) {
	    COPYFROMTO(new, Line, Length);
	    DISPOSE(Line);
	}
	Line = new;
	Length += len + MEM_INC;
    }

    for (q = &Line[Point], i = End - Point; --i >= 0; )
	q[len + i] = q[i];
    COPYFROMTO(&Line[Point], p, len);
    End += len;
    Line[End] = '\0';
    pos0 = screen_pos();
    pos1 = printlen((char *)&Line[Point]);
    TTYstring(&Line[Point]);
    Point += len;
    if ((pos0+pos1)%TTYwidth == 0)
	if (downline && RequireNLforWrap) TTYputs(downline);
    /* if the line is longer than TTYwidth this may put the cursor   */
    /* on the next line and confuse some other parts, so put it back */ 
    /* at Point                                                      */
    if (upline && (Point != End))
    {
	pos0 = screen_pos();
	pos1 = printlen((char *)&Line[Point]);
	for (i=((pos0%TTYwidth)+pos1)/TTYwidth; i > 0; i--)
	    if (upline) TTYputs(upline);
	TTYputs(bol);
	for (i=0 ; i < (pos0%TTYwidth); i++)
	    TTYputs(move_right);
    }

    return Point == End ? CSstay : CSmove;
}


STATIC ECHAR *next_hist()
{
    return H.Pos >= H.Size - 1 ? NULL : H.Lines[++H.Pos];
}

STATIC ECHAR *prev_hist()
{
    return H.Pos == 0 ? NULL : H.Lines[--H.Pos];
}

STATIC STATUS do_insert_hist(ECHAR *p)
{
    int i;
    if (p == NULL)
	return ring_bell();
    for (i=screen_pos()/TTYwidth; i > 0; i--)
	if (upline) TTYputs(upline);
    Point = 0;
    reposition(1);
    ceol();
    End = 0;
    return insert_string(p);
}

STATIC STATUS do_hist(ECHAR *(*move)())
{
    ECHAR	*p;
    int		i;

    i = 0;
    do {
	if ((p = (*move)()) == NULL)
	    return ring_bell();
    } while (++i < Repeat);
    return do_insert_hist(p);
}

STATIC STATUS h_next()
{
    return do_hist(next_hist);
}

STATIC STATUS h_prev()
{
    return do_hist(prev_hist);
}

STATIC STATUS h_first()
{
    return do_insert_hist(H.Lines[H.Pos = 0]);
}

STATIC STATUS h_last()
{
    return do_insert_hist(H.Lines[H.Pos = H.Size - 1]);
}

/*
**  Return zero if pat appears as a substring in text.
*/
STATIC int substrcmp(char *text, char *pat, int len)
{
    ECHAR	c;

    if ((c = *pat) == '\0')
        return *text == '\0';
    for ( ; *text; text++)
        if (*text == c && strncmp(text, pat, len) == 0)
            return 0;
    return 1;
}

STATIC ECHAR *search_hist(ECHAR *search, ECHAR *(*move)())
{
    static ECHAR	*old_search;
    int		len;
    int		pos;
    int		(*match)();
    char	*pat;

    /* Save or get remembered search pattern. */
    if (search && *search) {
	if (old_search)
	    DISPOSE(old_search);
	old_search = (ECHAR *)STRDUP((const char *)search);
    }
    else {
	if (old_search == NULL || *old_search == '\0')
            return NULL;
	search = old_search;
    }

    /* Set up pattern-finder. */
    if (*search == '^') {
	match = strncmp;
	pat = (char *)(search + 1);
    }
    else {
	match = substrcmp;
	pat = (char *)search;
    }
    len = strlen(pat);

    for (pos = H.Pos; (*move)() != NULL; )
	if ((*match)((char *)H.Lines[H.Pos], pat, len) == 0)
            return H.Lines[H.Pos];
    H.Pos = pos;
    return NULL;
}

STATIC STATUS h_search()
{
    static int	Searching;
    CONST char	*old_prompt;
    ECHAR	*(*move)();
    ECHAR	*p;

    if (Searching)
	return ring_bell();
    Searching = 1;

    clear_line();
    old_prompt = Prompt;
    Prompt = "Search: ";
    TTYputs((ECHAR *)Prompt);
    move = Repeat == NO_ARG ? prev_hist : next_hist;
    p = search_hist(editinput(), move);
    clear_line();
    Prompt = old_prompt;
    TTYputs((ECHAR *)Prompt);

    Searching = 0;
    return do_insert_hist(p);
}

STATIC STATUS fd_char()
{
    int		i;

    i = 0;
    do {
	if (Point >= End)
	    break;
	right(CSmove);
    } while (++i < Repeat);
    return CSstay;
}

STATIC void save_yank(int begin, int i)
{
    if (Yanked) {
	DISPOSE(Yanked);
	Yanked = NULL;
    }

    if (i < 1)
	return;

    if ((Yanked = NEW(ECHAR, (ESIZE_T)i + 1)) != NULL) {
	COPYFROMTO(Yanked, &Line[begin], i);
	Yanked[i] = '\0';
    }
}

STATIC STATUS delete_string(int count)
{
    int		i;
    int pos0,pos1,q;
    char	*tLine;

    if (count <= 0 || End == Point)
	return ring_bell();

    if (Point + count > End && (count = End - Point) <= 0)
	return CSstay;

    if (count > 1)
	save_yank(Point, count);

    tLine = STRDUP((char *)Line);
    ceol();
    for (q = Point, i = End - (Point + count) + 1; --i >= 0; q++)
	Line[q] = tLine[q+count];
    wfree(tLine);
    End -= count;
    pos0 = screen_pos();
    pos1 = printlen((char *)&Line[Point]);
    TTYstring(&Line[Point]);
    if ((pos1 > 0) && (pos0+pos1)%TTYwidth == 0)
	if (downline && RequireNLforWrap) TTYputs(downline);
    /* if the line is longer than TTYwidth this may put the cursor   */
    /* on the next line and confuse some other parts, so put it back */ 
    /* at Point                                                      */
    if (upline)
    {
	for (i=((pos0%TTYwidth)+pos1)/TTYwidth; i > 0; i--)
	    if (upline) TTYputs(upline);
	TTYputs(bol);
	for (i=0 ; i < (pos0%TTYwidth); i++)
	    TTYputs(move_right);
    }

    return CSmove;
}

STATIC STATUS bk_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS bk_del_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return delete_string(i);
}

STATIC STATUS redisplay()
{
    if (clrpage) TTYputs(clrpage);
    else
	TTYputs((ECHAR *)newline);
/*    TTYputs((ECHAR *)Prompt);
    TTYstring(Line); */
    return CSmove;
}

STATIC STATUS kill_line()
{
    int		i;

    if (Repeat != NO_ARG) {
	if (Repeat < Point) {
	    i = Point;
	    Point = Repeat;
	    reposition(1);
	    (void)delete_string(i - Point);
	}
	else if (Repeat > Point) {
	    right(CSmove);
	    (void)delete_string(Repeat - Point - 1);
	}
	return CSmove;
    }

    save_yank(Point, End - Point);
    ceol();
    Line[Point] = '\0';
    End = Point;
    return CSstay;
}

STATIC char *rsearch_hist(char *patt, int *lpos,int *cpos)
{
    /* Extension by awb to do reverse incremental searches */

    for (; *lpos > 0; (*lpos)--)
    {
	for ( ; (*cpos) >= 0 ; (*cpos)--)
	{
/*	    fprintf(stderr,"comparing %d %s %s\n",*lpos,patt,H.Lines[*lpos]+*cpos); */
	    if (strncmp(patt,(char *)H.Lines[*lpos]+*cpos,strlen(patt)) == 0)
	    {   /* found a match */
		return (char *)H.Lines[*lpos];
	    }
	}
	if ((*lpos) > 0)
	    *cpos = strlen((char *)H.Lines[(*lpos)-1]);
    }
    return NULL;  /* no match found */
}

STATIC STATUS h_risearch()
{
    STATUS	s;
    CONST char	*old_prompt;
    char *pat, *hist, *nhist;
    char *nprompt;
    int patend, i;
    ECHAR	c;
    int lpos,cpos;

    old_prompt = Prompt;

    nprompt = walloc(char,80+160);
    pat = walloc(char,80);
    patend=0;
    pat[0] = '\0';
    hist = "";
    lpos = H.Pos;   /* where the search has to start from */
    cpos = strlen((char *)H.Lines[lpos]);
    do 
    {
	sprintf(nprompt,"(reverse-i-search)`%s': ",pat);
	Prompt = nprompt;
	kill_line();
	do_insert_hist((ECHAR *)hist);
	if (patend != 0)
	    for (i=strlen((char *)H.Lines[lpos]); i>cpos; i--) bk_char();
	c = TTYget();
	if ((c >= ' ') || (c == CTL('R')))
	{
	    if (c == CTL('R'))
		cpos--;
	    else if (patend < 79)
	    {
		pat[patend]=c;
		patend++;
		pat[patend]='\0';
	    }
	    else  /* too long */
	    {
		ring_bell();
		continue;
	    }
	    nhist = rsearch_hist(pat,&lpos,&cpos);
	    if (nhist != NULL)
	    {
		hist = nhist;
		H.Pos = lpos;
	    }
	    else
	    {   /* oops, no match */
		ring_bell();
		if (c != CTL('R'))
		{
		    patend--;
		    pat[patend] = '\0';
		}
	    }
	}
    } while ((c >= ' ') || (c == CTL('R')));
    
    /* Tidy up */
    clear_line();
    Prompt = old_prompt;
    TTYputs((ECHAR *)Prompt);
    wfree(nprompt);

    kill_line();
    s = do_insert_hist((ECHAR *)hist);
    if (patend != 0)
	for (i=strlen((char *)H.Lines[lpos]); i>cpos; i--) s = bk_char();
    if (c != ESC)
	return emacs(c);
    else
	return s;
}

STATIC STATUS insert_char(int c)
{
    STATUS	s;
    ECHAR	buff[2];
    ECHAR	*p;
    ECHAR	*q;
    int		i;

    if (Repeat == NO_ARG || Repeat < 2) {
	buff[0] = c;
	buff[1] = '\0';
	return insert_string(buff);
    }

    if ((p = NEW(ECHAR, Repeat + 1)) == NULL)
	return CSstay;
    for (i = Repeat, q = p; --i >= 0; )
	*q++ = c;
    *q = '\0';
    Repeat = 0;
    s = insert_string(p);
    DISPOSE(p);
    return s;
}

STATIC STATUS meta()
{
    unsigned int	c;
    KEYMAP		*kp;

    if ((c = TTYget()) == EOF)
	return CSeof;
#if	defined(ANSI_ARROWS)
    /* Also include VT-100 arrows. */
    if (c == '[' || c == 'O')
	switch (c = TTYget()) {
	default:	return ring_bell();
	case EOF:	return CSeof;
	case 'A':	return h_prev();
	case 'B':	return h_next();
	case 'C':	return fd_char();
	case 'D':	return bk_char();
	}
#endif	/* defined(ANSI_ARROWS) */

    if (isdigit(c)) {
	for (Repeat = c - '0'; (c = TTYget()) != EOF && isdigit(c); )
	    Repeat = Repeat * 10 + c - '0';
	el_Pushed = 1;
	el_PushBack = c;
	return CSstay;
    }

/*    if (isupper(c))
         return do_macro(c); */
    for (OldPoint = Point, kp = MetaMap; kp->Function; kp++)
	if (kp->Key == c)
	    return (*kp->Function)();
    if (rl_meta_chars == 0)
    {
	insert_char(META(c));
	return CSmove;
    }

    return ring_bell();
}

STATIC STATUS emacs(unsigned int c)
{
    STATUS		s;
    KEYMAP		*kp;

    if (ISMETA(c) && rl_meta_chars) 
    {    
	el_Pushed = 1;
	el_PushBack = UNMETA(c);
	return meta();
    }
    for (kp = Map; kp->Function; kp++)
	if (kp->Key == c)
	    break;
    s = kp->Function ? (*kp->Function)() : insert_char((int)c);
    if (!el_Pushed)
	/* No pushback means no repeat count; hacky, but true. */
	Repeat = NO_ARG;
    return s;
}

STATIC STATUS TTYspecial(unsigned int c)
{
    int i;
    
    if (ISMETA(c))
	return CSdispatch;

    if (c == rl_erase || c == DEL)
	return bk_del_char();
    if (c == rl_kill) {
	if (Point != 0) {
	    for (i=screen_pos()/TTYwidth; i > 0; i--)
		if (upline) TTYputs(upline);
	    Point = 0;
	    reposition(1);
	}
	Repeat = NO_ARG;
	return kill_line();
    }
    if (c == rl_intr || c == rl_quit) {
	Point = End = 0;
	Line[0] = '\0';
	if (c == rl_intr) 
	{
	    el_intr_pending = 1;
	    return CSdone;
	}
	else 
	    return redisplay();
    }
    if (c == rl_eof && Point == 0 && End == 0)
	return CSeof;

    return CSdispatch;
}

STATIC ECHAR *editinput()
{
    unsigned int	c;

    Repeat = NO_ARG;
    OldPoint = Point = Mark = End = 0;
    Line[0] = '\0';

    while ((c = TTYget()) != EOF)
      {
	switch (TTYspecial(c)) {
	case CSdone:
	    return Line;
	case CSeof:
	    return NULL;
	case CSmove:
	    reposition(1);
	    break;
	case CSstay:
	    break;
	case CSdispatch:
	    switch (emacs(c)) {
	    case CSdone:
		return Line;
	    case CSeof:
		return NULL;
	    case CSmove:
		reposition(1);
		break;
	    case CSstay:
	    case CSdispatch:
		break;
	    }
	    break;
	}
      }
    return NULL;
}

STATIC void hist_add(ECHAR *p)
{
    int		i;

    if ((p = (ECHAR *)STRDUP((char *)p)) == NULL)
	return;
    if (H.Size < editline_histsize)
	H.Lines[H.Size++] = p;
    else {
	DISPOSE(H.Lines[0]);
	for (i = 0; i < editline_histsize - 1; i++)
	    H.Lines[i] = H.Lines[i + 1];
	H.Lines[i] = p;
    }
    H.Pos = H.Size - 1;
}

/* Added by awb 29/12/98 to get saved history file */
void write_history(const char *history_file)
{
    FILE *fd;
    int i;

    if ((fd = fopen(history_file,"wb")) == NULL)
    {
	fprintf(stderr,"editline: can't access history file \"%s\"\n",
		history_file);
	return;
    }

    for (i=0; i < H.Size; i++)
	fprintf(fd,"%s\n",H.Lines[i]);
    fclose(fd);
}

void read_history(const char *history_file)
{
    FILE *fd;
    char buff[2048];
    int c,i;

    H.Lines = NEW(ECHAR *,editline_histsize);
    H.Size = 0;
    H.Pos = 0;
    
    if ((fd = fopen(history_file,"rb")) == NULL)
	return; /* doesn't have a history file yet */

    while ((c=getc(fd)) != EOF)
    {
	ungetc(c,fd);
	for (i=0; ((c=getc(fd)) != '\n') && (c != EOF); i++)
	    if (i < 2047)
		buff[i] = c;
	buff[i] = '\0';
	add_history(buff);
    }

    fclose(fd);
}

/*
**  For compatibility with FSF readline.
*/
/* ARGSUSED0 */
void
rl_reset_terminal(char *p)
{
}

void
rl_initialize()
{
}

char *readline(CONST char *prompt)
{
    ECHAR	*line;

    if (Line == NULL) {
	Length = MEM_INC;
	if ((Line = NEW(ECHAR, Length)) == NULL)
	    return NULL;
    }

    TTYinfo();
    rl_ttyset(0);
    hist_add(el_NIL);
    ScreenSize = SCREEN_INC;
    Screen = NEW(char, ScreenSize);
    Prompt = prompt ? prompt : (char *)el_NIL;
    el_intr_pending = 0;
    if (el_no_echo == 1)
    {
	el_no_echo = 0;
	TTYputs((ECHAR *)Prompt);
	TTYflush();
	el_no_echo = 1;
    }
    else
	TTYputs((ECHAR *)Prompt);
    line = editinput();
    if (line != NULL) {
	line = (ECHAR *)STRDUP((char *)line);
	TTYputs((ECHAR *)newline);
	TTYflush();
    }
    rl_ttyset(1);
    DISPOSE(Screen);
    DISPOSE(H.Lines[--H.Size]);
    if (el_intr_pending)
	do_user_intr();
    return (char *)line;
}

void
add_history(p)
    char	*p;
{
    if (p == NULL || *p == '\0')
	return;

#if	defined(UNIQUE_HISTORY)
    if (H.Pos && strcmp(p, H.Lines[H.Pos - 1]) == 0)
        return;
#endif	/* defined(UNIQUE_HISTORY) */
    hist_add((ECHAR *)p);
}


STATIC STATUS beg_line()
{
    int i;
    if (Point) {
	for (i=screen_pos()/TTYwidth; i > 0; i--)
	    if (upline) TTYputs(upline);
	Point = 0;
	return CSmove;
    }
    return CSstay;
}

STATIC STATUS del_char()
{
    return delete_string(Repeat == NO_ARG ? 1 : Repeat);
}

STATIC STATUS end_line()
{
    if (Point != End) {
	while (Point < End)
	{
	    TTYput(Line[Point]);
	    Point++;
	}
	return CSmove;
    }
    return CSstay;
}

/*
**  Move back to the beginning of the current word and return an
**  allocated copy of it.
*/
STATIC ECHAR *find_word()
{
    static char	SEPS[] = "#;&|^$=`'{}()<>\n\t ";
    ECHAR	*p;
    ECHAR	*new;
    ESIZE_T	len;

    for (p = &Line[Point]; p > Line && strchr(SEPS, (char)p[-1]) == NULL; p--)
	continue;
    len = Point - (p - Line) + 1;
    if ((new = NEW(ECHAR, len)) == NULL)
	return NULL;
    COPYFROMTO(new, p, len);
    new[len - 1] = '\0';
    return new;
}

void el_redisplay()
{
    reposition(0);  /* redisplay assuming already on newline */
}

char *el_current_sym()
{
    /* Get current symbol at point -- awb*/
    char *symbol = NULL;
    int i,j;

    if (End == 0)
	return NULL;
    if (Point == End)
	i=Point-1;
    else
	i=Point;
	
    for ( ;
	 ((i >= 0) &&
	  (strchr("()' \t\n\r",Line[i]) != NULL));
	 i--);
    /* i will be on final or before final character */
    if (i < 0)
	return NULL;
    /* But if its not at the end of the current symbol move it there */
    for (; i < End; i++)
	if (strchr("()' \t\n\r\"",Line[i]) != NULL)
	    break;
    for (j=i-1; j >=0; j--)
	if (strchr("()' \t\n\r\"",Line[j]) != NULL)
	    break;

    symbol = walloc(char,i-j);
    strncpy(symbol,(char *)&Line[j+1],i-(j+1));
    symbol[i-(j+1)] = '\0';

    return symbol;
}

static char *completion_to_ambiguity(int index,char **possibles)
{
    /* Find the string that extends from index in possibles until an */
    /* ambiguity is found                           -- awb     */
    char *p;
    int e,i;
    int extending;

    extending = 1;
    e = index;

    for ( ; extending; e++)
    {
	for (i=0; possibles[i] != NULL; i++)
	    if (possibles[i][e] != possibles[0][e])
	    {
		extending = 0;
		e--;
		break;
	    }
    }

    if (e==index)
	return NULL;  /* already at ambiguity */
    else
    {
	p = walloc(char,(e-index)+1);
 	strncpy(p,possibles[0]+index,e-index);
	p[e-index] = '\0';
	return p;
    }
}

static char **el_file_completion_function(char * text, int start, int end)
{
    /* Interface to editline rl_list_possib which looks up possible */
    /* file name completions.                                       */
    char *word;
    char **matches1;
    char **matches2;
    int ac,i;

    word = walloc(char,(end-start)+1);
    strncpy(word,text+start,end-start);
    word[end-start]='\0';

    ac = rl_list_possib(word,&matches1);
    wfree(word);
    if (ac == 0)
	return NULL;
    else
    {
	matches2 = walloc(char *,ac+1);
	for (i=0; i < ac; i++)
	    matches2[i] = matches1[i];
	matches2[i] = NULL;
	wfree(matches1);
	return matches2;
    }
}

STATIC STATUS c_complete()
{
    /* Modified by awb 30/12/98 to allow listing of possibles and */
    /* a user definable completion method                         */
    char	*p;
    char	*word;
    int		start;
    char        **possibles=NULL;
    int         possiblesc=0;
    int started_with_quote = 0;
    STATUS	s;
    int i;

    for (start=Point; start > 0; start--)
	if (strchr("()' \t\n\r\"",Line[start-1]) != NULL)
	    break;
    word = walloc(char,(Point-start)+1);
    strncpy(word,(char *)(Line+start),Point-start);
    word[Point-start]='\0';
    if ((start > 0) && (Line[start-1] == '"'))
	started_with_quote = 1;

    if (el_user_completion_function)
	/* May need to look at previous char so pass in Line */
	possibles = el_user_completion_function((char *)Line,start,Point);
    if (possibles == NULL)
    {
	possibles = el_file_completion_function((char *)Line,start,Point);
	/* As filename completions only complete the final file name */
	/* not the full path we need to set a new start position     */
	for (start=Point; start > 0; start--)
	    if (strchr("()' \t\n\r\"/",Line[start-1]) != NULL)
		break;
    }
    if (possibles)
	for (possiblesc=0; possibles[possiblesc] != NULL; possiblesc++);
    
    if ((!possibles) || (possiblesc == 0)) /* none or none at all */
	s = ring_bell();
    else if (possiblesc == 1)  /* a single expansion */
    {
	p = walloc(char,strlen(possibles[0])-(Point-start)+2);
	sprintf(p,"%s ",possibles[0]+(Point-start));
	if ((strlen(p) > 1) && (p[strlen(p)-2] == '/'))
	    p[strlen(p)-1] = '\0';
	else if (started_with_quote)
	    p[strlen(p)-1] = '"';
	
	s = insert_string((ECHAR *)p);
	wfree(p);
    }
    else if ((p = completion_to_ambiguity(Point-start,possibles)) != NULL)
    {    /* an expansion to a later ambiguity */
	s = insert_string((ECHAR *)p);
	wfree(p);
	ring_bell();
    }
    else /* list of possibilities and we can't expand any further */
    {
	print_columns(possiblesc,possibles);  /* display options */
	reposition(0);  /* display whole line again */
	s = CSmove;
    }

    for (i=0; possibles && possibles[i] != NULL; i++)
	wfree(possibles[i]);
    wfree(possibles);
    wfree(word);

    return s;
}

#if 0
/* Original version without automatic listing of possible completions */
STATIC STATUS c_complete_old()
{
    ECHAR	*p;
    ECHAR	*word;
    int		unique;
    STATUS	s;

    word = find_word();
    p = (ECHAR *)rl_complete((char *)word, &unique);
    if (word)
	DISPOSE(word);
    if (p && *p) {
	s = insert_string(p);
	if (!unique)
	    (void)ring_bell();
	DISPOSE(p);
	return s;
    }
    return ring_bell();
}
#endif

STATIC STATUS c_possible()
{
    ECHAR	**av;
    ECHAR	*word;
    int		ac;

    word = find_word();
    /* The (char ***) ((void *) &av) below is to avoid a warning
     * from GCC about casting an unsigned char *** to char ***
     */
    ac = rl_list_possib((char *)word, (char ***) ((void *) &av));
    if (word)
	DISPOSE(word);
    if (ac) {
	print_columns(ac, (char **)av);
	reposition(0);
	while (--ac >= 0)
	    DISPOSE(av[ac]);
	DISPOSE(av);
	return CSmove;
    }
    return ring_bell();
}

STATIC STATUS accept_line()
{
    Line[End] = '\0';
    return CSdone;
}

#ifdef SYSTEM_IS_WIN32
STATIC STATUS end_of_input()
{
    Line[End] = '\0';
    return CSeof;
}
#endif

STATIC STATUS transpose()
{
    ECHAR	c;

    if (Point) {
	if (Point == End)
	    left(CSmove);
	c = Line[Point - 1];
	left(CSstay);
	Line[Point - 1] = Line[Point];
	TTYshow(Line[Point - 1]);
	Line[Point++] = c;
	TTYshow(c);
    }
    return CSstay;
}

STATIC STATUS quote()
{
    unsigned int	c;

    return (c = TTYget()) == EOF ? CSeof : insert_char((int)c);
}

STATIC STATUS wipe()
{
    int		i;

    if (Mark > End)
	return ring_bell();

    if (Point > Mark) {
	i = Point;
	Point = Mark;
	Mark = i;
	reposition(1);
    }

    return delete_string(Mark - Point);
}

STATIC STATUS mk_set()
{
    Mark = Point;
    return CSstay;
}

STATIC STATUS exchange()
{
    unsigned int	c;

    if ((c = TTYget()) != CTL('X'))
	return c == EOF ? CSeof : ring_bell();

    if ((c = Mark) <= End) {
	Mark = Point;
	Point = c;
	return CSmove;
    }
    return CSstay;
}

STATIC STATUS yank()
{
    if (Yanked && *Yanked)
	return insert_string(Yanked);
    return CSstay;
}

STATIC STATUS copy_region()
{
    if (Mark > End)
	return ring_bell();

    if (Point > Mark)
	save_yank(Mark, Point - Mark);
    else
	save_yank(Point, Mark - Point);

    return CSstay;
}

STATIC STATUS move_to_char()
{
    unsigned int	c;
    int			i;
    ECHAR		*p;

    if ((c = TTYget()) == EOF)
	return CSeof;
    for (i = Point + 1, p = &Line[i]; i < End; i++, p++)
	if (*p == c) {
	    Point = i;
	    return CSmove;
	}
    return CSstay;
}

STATIC STATUS fd_word()
{
    return do_forward(CSmove);
}

STATIC STATUS fd_kill_word()
{
    int		i;
    int OP;

    OP = Point;
    (void)do_forward(CSmove);
    if (OP != Point) {
	i = Point - OP;
	for ( ; Point > OP; Point --)
	    TTYback();
	return delete_string(i);
    }
    return CSmove;
}

STATIC STATUS bk_word()
{
    int		i;
    ECHAR	*p;

    i = 0;
    do {
	for (p = &Line[Point]; p > Line && !isalnum(p[-1]); p--)
	    left(CSmove);

	for (; p > Line && p[-1] != ' ' && isalnum(p[-1]); p--)
	    left(CSmove);

	if (Point == 0)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

STATIC STATUS bk_kill_word()
{
    (void)bk_word();
    if (OldPoint != Point)
	return delete_string(OldPoint - Point);
    return CSstay;
}

STATIC int argify(ECHAR *line, ECHAR ***avp)
{
    ECHAR	*c;
    ECHAR	**p;
    ECHAR	**new;
    int		ac;
    int		i;

    i = MEM_INC;
    if ((*avp = p = NEW(ECHAR*, i))== NULL)
	 return 0;

    for (c = line; isspace(*c); c++)
	continue;
    if (*c == '\n' || *c == '\0')
	return 0;

    for (ac = 0, p[ac++] = c; *c && *c != '\n'; ) {
	if (isspace(*c)) {
	    *c++ = '\0';
	    if (*c && *c != '\n') {
		if (ac + 1 == i) {
		    new = NEW(ECHAR*, i + MEM_INC);
		    if (new == NULL) {
			p[ac] = NULL;
			return ac;
		    }
		    COPYFROMTO(new, p, i * sizeof (char **));
		    i += MEM_INC;
		    DISPOSE(p);
		    *avp = p = new;
		}
		p[ac++] = c;
	    }
	}
	else
	    c++;
    }
    *c = '\0';
    p[ac] = NULL;
    return ac;
}

STATIC STATUS last_argument()
{
    ECHAR	**av;
    ECHAR	*p;
    STATUS	s;
    int		ac;

    if (H.Size == 1 || (p = H.Lines[H.Size - 2]) == NULL)
	return ring_bell();

    if ((p = (ECHAR *)STRDUP((char *)p)) == NULL)
	return CSstay;
    ac = argify(p, &av);

    if (Repeat != NO_ARG)
	s = Repeat < ac ? insert_string(av[Repeat]) : ring_bell();
    else
	s = ac ? insert_string(av[ac - 1]) : CSstay;

    if (ac)
	DISPOSE(av);
    DISPOSE(p);
    return s;
}

STATIC KEYMAP	Map[33] = {
    {	CTL('@'),	ring_bell	},
    {	CTL('A'),	beg_line	},
    {	CTL('B'),	bk_char		},
    {	CTL('D'),	del_char	},
    {	CTL('E'),	end_line	},
    {	CTL('F'),	fd_char		},
    {	CTL('G'),	ring_bell	},
    {	CTL('H'),	bk_del_char	},
    {	CTL('I'),	c_complete	},
    {	CTL('J'),	accept_line	},
    {	CTL('K'),	kill_line	},
    {	CTL('L'),	redisplay	},
    {	CTL('M'),	accept_line	},
    {	CTL('N'),	h_next		},
    {	CTL('O'),	ring_bell	},
    {	CTL('P'),	h_prev		},
    {	CTL('Q'),	ring_bell	},
    {	CTL('R'),	h_risearch	},
    {	CTL('S'),	h_search	},
    {	CTL('T'),	transpose	},
    {	CTL('U'),	ring_bell	},
    {	CTL('V'),	quote		},
    {	CTL('W'),	wipe		},
    {	CTL('X'),	exchange	},
    {	CTL('Y'),	yank		},
#ifdef SYSTEM_IS_WIN32
    {	CTL('Z'),	end_of_input	},
#else
    {	CTL('Z'),	ring_bell	},
#endif
    {	CTL('['),	meta		},
    {	CTL(']'),	move_to_char	},
    {	CTL('^'),	ring_bell	},
    {	CTL('_'),	ring_bell	},
    {	0,		NULL		}
};

STATIC KEYMAP	MetaMap[64]= {
    {	CTL('H'),	bk_kill_word	},
    {	DEL,		bk_kill_word	},
    {	' ',		mk_set	},
    {	'.',		last_argument	},
    {	'<',		h_first		},
    {	'>',		h_last		},
    {	'?',		c_possible	},
    {	'b',		bk_word		},
    {	'c',		case_cap_word	},
    {	'd',		fd_kill_word	},
    {	'f',		fd_word		},
    {	'l',		case_down_word	},
    {	'u',		case_up_word	},
    {	'y',		yank		},
    {	'w',		copy_region	},
    {	0,		NULL		}
};

void el_bind_key_in_metamap(char c, Keymap_Function func)
{
    /* Add given function to key map for META keys */
    int i;

    for (i=0; MetaMap[i].Key != 0; i++)
    {
	if (MetaMap[i].Key == c)
	{
	    MetaMap[i].Function = func;
	    return;
	}
    }

    /* A new key so have to add it to end */
    if (i == 63)
    {
	fprintf(stderr,"editline: MetaMap table full, requires increase\n");
	return;
    }
    
    MetaMap[i].Function = func;
    MetaMap[i].Key = c;
    MetaMap[i+1].Function = 0;  /* Zero the last location */
    MetaMap[i+1].Key = 0;       /* Zero the last location */

}


