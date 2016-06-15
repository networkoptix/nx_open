/* Scheme In One Defun, but in C this time.
 
 *                      COPYRIGHT (c) 1988-1994 BY                          *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *			   ALL RIGHTS RESERVED                              *

Permission to use, copy, modify, distribute and sell this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all copies
and that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Paradigm Associates
Inc not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

PARADIGM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
PARADIGM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/*

gjc@paradigm.com, gjc@mitech.com

Paradigm Associates Inc          Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138


   Release 1.0: 24-APR-88
   Release 1.1: 25-APR-88, added: macros, predicates, load. With additions by
    Barak.Pearlmutter@DOGHEN.BOLTZ.CS.CMU.EDU: Full flonum recognizer,
    cleaned up uses of NULL/0. Now distributed with siod.scm.
   Release 1.2: 28-APR-88, name changes as requested by JAR@AI.AI.MIT.EDU,
    plus some bug fixes.
   Release 1.3: 1-MAY-88, changed env to use frames instead of alist.
    define now works properly. vms specific function edit.
   Release 1.4 20-NOV-89. Minor Cleanup and remodularization.
    Now in 3 files, siod.h, slib.c, siod.c. Makes it easier to write your
    own main loops. Some short-int changes for lightspeed C included.
   Release 1.5 29-NOV-89. Added startup flag -g, select stop and copy
    or mark-and-sweep garbage collection, which assumes that the stack/register
    marking code is correct for your architecture. 
   Release 2.0 1-DEC-89. Added repl_hooks, Catch, Throw. This is significantly
    different enough (from 1.3) now that I'm calling it a major release.
   Release 2.1 4-DEC-89. Small reader features, dot, backquote, comma.
   Release 2.2 5-DEC-89. gc,read,print,eval, hooks for user defined datatypes.
   Release 2.3 6-DEC-89. save_forms, obarray intern mechanism. comment char.
   Release 2.3a......... minor speed-ups. i/o interrupt considerations.
   Release 2.4 27-APR-90 gen_readr, for read-from-string.
   Release 2.5 18-SEP-90 arrays added to SIOD.C by popular demand. inums.
   Release 2.6 11-MAR-92 function prototypes, some remodularization.
   Release 2.7 20-MAR-92 hash tables, fasload. Stack check.
   Release 2.8  3-APR-92 Bug fixes, \n syntax in string reading.
   Release 2.9 28-AUG-92 gc sweep bug fix. fseek, ftell, etc. Change to
    envlookup to allow (a . rest) suggested by bowles@is.s.u-tokyo.ac.jp.
   Release 2.9a 10-AUG-93. Minor changes for Windows NT.
   Release 3.0  12-JAN-94. Release it, include changes/cleanup recommended by
    andreasg@nynexst.com for the OS2 C++ compiler. Compilation and running
    tested using DEC C, VAX C. WINDOWS NT. GNU C on SPARC.

   Festival/Edinburgh Speech Tools changes (awb@cstr.ed.ac.uk) 1996-1999
   Note there have been substantial changes to this from its original
   form which may have introduced bugs.  Please contact Alan W Black
   (awb@cstr.ed.ac.uk) first if you find problems unless you can confirm
   they also exist in the original siod-3.0 release

   March 1999 split off functions into different files to make it easier
   for our documentation purposes, sorry maybe this should be called 
   SNIOD now :-), or maybe Scheme in one Directory.

  */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "EST_unix.h"

#include "EST_cutils.h"
#include "siod.h"
#include "siodp.h"

#ifdef WIN32
#include "winsock2.h"
#endif

static int restricted_function_call(LISP l);
static long repl(struct repl_hooks *h);
static void gc_mark_and_sweep(void);
static void gc_ms_stats_start(void);
static void gc_ms_stats_end(void);
static void mark_protected_registers(void);
static void mark_locations(LISP *start,LISP *end);
static void gc_sweep(void);
static void mark_locations_array(LISP *x,long n);
static LISP lreadr(struct gen_readio *f);
static LISP lreadparen(struct gen_readio *f);
static LISP lreadstring(struct gen_readio *f);

const char *siod_version(void)
{return("3.0 FIELD TEST");}

LISP heap_1,heap_2;
LISP heap,heap_end,heap_org;
long heap_size = DEFAULT_HEAP_SIZE;
long old_heap_used;
long which_heap;
long gc_status_flag = 0;
long show_backtrace = 0;
char *init_file = (char *) NULL;
char *tkbuffer = NULL;
long gc_kind_copying = 0;
long gc_cells_allocated = 0;
double gc_time_taken;
LISP *stack_start_ptr;
LISP freelist;

long nointerrupt = 1;
long interrupt_differed = 0;
LISP oblistvar = NIL;
LISP current_env = NIL;
static LISP siod_backtrace = NIL;
LISP restricted = NIL;
LISP truth = NIL;
LISP eof_val = NIL;
LISP sym_errobj = NIL;
LISP sym_quote = NIL;
LISP sym_dot = NIL;
LISP unbound_marker = NIL;
LISP *obarray;
long obarray_dim = 100;
struct catch_frame *catch_framep = (struct catch_frame *) NULL;
void (*repl_puts)(char *) = NULL;
LISP (*repl_read)(void) = NULL;
LISP (*repl_eval)(LISP) = NULL;
void (*repl_print)(LISP) = NULL;
repl_getc_fn siod_fancy_getc = f_getc;
repl_ungetc_fn siod_fancy_ungetc = f_ungetc;
LISP *inums;
LISP siod_docstrings = NIL;  /* for builtin functions */
long inums_dim = 100;
struct user_type_hooks *user_types = NULL;
struct gc_protected *protected_registers = NULL;
jmp_buf save_regs_gc_mark;
double gc_rt;
long gc_cells_collected;
static const char *user_ch_readm = "";
static const char *user_te_readm = "";
LISP (*user_readm)(int, struct gen_readio *) = NULL;
LISP (*user_readt)(char *,long, int *) = NULL;
void (*fatal_exit_hook)(void) = NULL;
#ifdef THINK_C
int ipoll_counter = 0;
#endif
FILE *fwarn=NULL;
int siod_interactive = 1;

extern "C" {
int el_pos = -1;  // actually used by readline 
}
const char *repl_prompt = "siod>";
const char *siod_prog_name = "siod";
const char *siod_primary_prompt = "siod> ";
const char *siod_secondary_prompt = "> ";

// A list of objects with gc_free_once set in their user_type_hooks structure
// whose gc_free function has been called in the current GC sweep.
void **dead_pointers = NULL;	
int size_dead_pointers = 0;
int num_dead_pointers = 0;
#define DEAD_POINTER_GROWTH (10)

static LISP set_restricted(LISP l);

char *stack_limit_ptr = NULL;
long stack_size = 
#ifdef THINK_C
  10000;
#else
  500000;
#endif

void NNEWCELL(LISP *_into,long _type)
{if NULLP(freelist)               
        {
             gc_for_newcell();              
        }
    *_into = freelist;                
    freelist = CDR(freelist);        
    ++gc_cells_allocated;
    
    (*_into)->gc_mark = 0;               
    (*_into)->type = (short) _type;
}

void need_n_cells(int n)
{
    /* Check there are N cells available, and force gc if not */
    LISP x = NIL;
    int i;

    for (i=0; i<n; i++)
        x = cons(NIL,x);

    return;
}

static void start_rememberring_dead(void)
{
  num_dead_pointers=0;
}

static int is_dead(void *ptr)
{
  int i;
  for(i=0; i<num_dead_pointers; i++)
    if (dead_pointers[i] == ptr)
      return 1;
  return 0;
}

static void mark_as_dead(void *ptr)
{
  int i;
  if (num_dead_pointers == size_dead_pointers)
      dead_pointers = wrealloc(dead_pointers, void *, size_dead_pointers += DEAD_POINTER_GROWTH);

  for(i=0; i<num_dead_pointers; i++)
    if (dead_pointers[i] == ptr)
      return;

  dead_pointers[num_dead_pointers++] = ptr;
}

void siod_print_welcome(EST_String extra_info)
{printf("Welcome to SIOD, Scheme In One Defun, Version %s\n",
	siod_version());
 printf("(C) Copyright 1988-1994 Paradigm Associates Inc.\n");
 if (extra_info != "")
   printf("%s\n", (const char *)extra_info);
}

void siod_print_welcome(void)
{
  siod_print_welcome("");
}

void print_hs_1(void)
{printf("heap_size = %ld cells, %ld bytes. %ld inums. GC is %s\n",
        heap_size,(long)(heap_size*sizeof(struct obj)),
	inums_dim,
	(gc_kind_copying == 1) ? "stop and copy" : "mark and sweep");}

void print_hs_2(void)
{if (gc_kind_copying == 1)
   printf("heap_1 at %p, heap_2 at %p\n",(void *)heap_1,(void *)heap_2);
 else
   printf("heap_1 at %p\n",(void *)heap_1);}

/* I don't have a clean way to do this but need to reset this if */
/* ctrl-c occurs. */
int audsp_mode = FALSE;
int siod_ctrl_c = FALSE;

static void err_ctrl_c(void)
{   
    audsp_mode = FALSE;
    siod_ctrl_c = TRUE;
    err("control-c interrupt",NIL);}

long no_interrupt(long n)
{long x;
 x = nointerrupt;
 nointerrupt = n;
 if ((nointerrupt == 0) && (interrupt_differed == 1))
   {interrupt_differed = 0;
    err_ctrl_c();}
 return(x);}

extern "C" void handle_sigfpe(int sig SIG_restargs)
{(void)sig;
 signal(SIGFPE,handle_sigfpe);
 /* Solaris seems to need a relse before it works again */
#ifdef __svr4__
 sigrelse(SIGFPE);
#endif
 /* linux needs to unmask sigfpe to allow for next one */
#ifdef __linux__
 sigset_t set1;
 sigemptyset(&set1);
 sigaddset(&set1,SIGFPE);
 sigprocmask(SIG_UNBLOCK,&set1,NULL);
#endif
 signal(SIGFPE,handle_sigfpe);
 err("floating point exception",NIL);}

extern "C" void handle_sigint(int sig SIG_restargs)
{(void)sig;
 signal(SIGINT,handle_sigint);
 /* Solaris seems to need a relse before it works again */
#ifdef __svr4__
 sigrelse(SIGINT);
#endif
 /* linux needs to unmask sigint to allow for next one */
#ifdef __linux__
 sigset_t set1;
 sigemptyset(&set1);
 sigaddset(&set1,SIGINT);
 sigprocmask(SIG_UNBLOCK,&set1,NULL);
#endif
 signal(SIGINT,handle_sigint);
 if (nointerrupt == 1)
   interrupt_differed = 1;
 else
   err_ctrl_c();}

void siod_reset_prompt(void)
{ 
    el_pos = -1;  /* flush remaining input on that line */
    repl_prompt = siod_primary_prompt;
    interrupt_differed = 0;
    nointerrupt = 0;
}

long repl_driver(long want_sigint,long want_init,struct repl_hooks *h)
{int k;
 struct repl_hooks hd;
 LISP stack_start;
 stack_start_ptr = &stack_start;
 stack_limit_ptr = STACK_LIMIT(stack_start_ptr,stack_size);
 est_errjmp = walloc(jmp_buf,1);
 k = setjmp(*est_errjmp);
 if(k) 
 {
     sock_acknowledge_error();  /* if there is a client let them know */
     siod_reset_prompt();
 }
 if (k == 2) return(2);
 siod_ctrl_c = FALSE;
 if (want_sigint) signal(SIGINT,handle_sigint);
 close_open_files();
 catch_framep = (struct catch_frame *) NULL;
 errjmp_ok = 1;
 interrupt_differed = 0;
 nointerrupt = 0;
 if (want_init && init_file && (k == 0)) vload(init_file,0);
 // Can't see where else to put this
 if ((siod_interactive) && (!isatty(0)))
 {   //  editline (or its replacement) would do this if stdin was a terminal
     fprintf(stdout,"%s",repl_prompt);
     fflush(stdout);
 }
 if (!h)
   {hd.repl_puts = repl_puts;
    hd.repl_read = repl_read;
    hd.repl_eval = repl_eval;
    hd.repl_print = repl_print;
    return(repl(&hd));}
 else
   return(repl(h));}

static void ignore_puts(char *st)
{(void)st;}

static void noprompt_puts(char *st)
{if (strcmp(st,"> ") != 0)
   put_st(st);}

static char *repl_c_string_arg = NULL;
static long repl_c_string_flag = 0;

static LISP repl_c_string_read(void)
{LISP s;
 if (repl_c_string_arg == NULL)
   return(eof_val);
 s = strcons(strlen(repl_c_string_arg),repl_c_string_arg);
 repl_c_string_arg = NULL;
 return(read_from_string(get_c_string(s)));}

static void ignore_print(LISP x)
{(void)x;
 repl_c_string_flag = 1;}

static void not_ignore_print(LISP x)
{repl_c_string_flag = 1;
 pprint(x);}

long repl_c_string(char *str,
		   long want_sigint,long want_init,long want_print)
{struct repl_hooks h;
 long retval;
 if (want_print)
   h.repl_puts = noprompt_puts;
 else
   h.repl_puts = ignore_puts;
 h.repl_read = repl_c_string_read;
 h.repl_eval = NULL;
 if (want_print)
   h.repl_print = not_ignore_print;
 else
   h.repl_print = ignore_print;
 repl_c_string_arg = str;
 repl_c_string_flag = 0;
 retval = repl_driver(want_sigint,want_init,&h);
 if (retval != 0)
   return(retval);
 else if (repl_c_string_flag == 1)
   return(0);
 else
   return(2);}

#ifdef unix
#include <sys/types.h>
#include <sys/times.h>
double myruntime(void)
{double total;
 struct tms b;
 times(&b);
 total = b.tms_utime;
 total += b.tms_stime;
 return(total / 60.0);}
#else
#if defined(THINK_C) | defined(WIN32) | defined(VMS)
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC CLK_TCK
#endif
double myruntime(void)
{return(((double) clock()) / ((double) CLOCKS_PER_SEC));}
#else
double myruntime(void)
{time_t x;
 time(&x);
 return((double) x);}
#endif
#endif

void set_repl_hooks(void (*puts_f)(char *),
		    LISP (*read_f)(void),
		    LISP (*eval_f)(LISP),
		    void (*print_f)(LISP))
{repl_puts = puts_f;
 repl_read = read_f;
 repl_eval = eval_f;
 repl_print = print_f;}

void fput_st(FILE *f,const char *st)
{long flag;
 if (f != NULL)  /* so we can block warning messages easily */
 {
     flag = no_interrupt(1);
     fprintf(f,"%s",st);
     no_interrupt(flag);
 }
}

void put_st(const char *st)
{fput_st(stdout,st);}
     
void grepl_puts(char *st,void (*repl_putss)(char *))
{if (repl_putss == NULL)
   {fput_st(fwarn,st);
    if (fwarn != NULL) fflush(stdout);}
 else
   (*repl_putss)(st);}

static void display_backtrace(LISP args)
{
    /* Display backtrace information */
    LISP l;
    int i;
    int local_show_backtrace = show_backtrace;
    show_backtrace = 0;  // so we don't recurse if an error occurs

    if (cdr(args) == NIL)
    {
	printf("BACKTRACE:\n");
	for (i=0,l=siod_backtrace; l != NIL; l=cdr(l),i++)
	{
	    fprintf(stdout,"%4d: ",i);
	    pprintf(stdout,car(l),3,72,2,2);
	    fprintf(stdout,"\n");
	}
    }
    else if (FLONUMP(car(cdr(args))))
    {
	printf("BACKTRACE:\n");
	int nth = (int)FLONM(car(cdr(args)));
	LISP frame = siod_nth(nth,siod_backtrace);
	fprintf(stdout,"%4d: ",nth);
	pprintf(stdout,frame,3,72,-1,-1);
	fprintf(stdout,"\n");
    }

    show_backtrace = local_show_backtrace;
}
     
static long repl(struct repl_hooks *h)
{LISP x,cw = 0;
 double rt;
 gc_kind_copying = 0;
 while(1)
   {
#if 0
    if ((gc_kind_copying == 1) && ((gc_status_flag) || heap >= heap_end))
     {rt = myruntime();
      gc_stop_and_copy();
      sprintf(tkbuffer,
	      "GC took %g seconds, %ld compressed to %ld, %ld free\n",
	      myruntime()-rt,old_heap_used,
	      (long)(heap-heap_org),(long)(heap_end-heap));
      grepl_puts(tkbuffer,h->repl_puts);}
    /* grepl_puts("> ",h->repl_puts); */
#endif
    if (h->repl_read == NULL)
      x = lread();
    else
      x = (*h->repl_read)();
    if EQ(x,eof_val) break;
    rt = myruntime();
    if (gc_kind_copying == 1)
      cw = heap;
    else
      {gc_cells_allocated = 0;
       gc_time_taken = 0.0;}
    /* Check if its a debugger command */
    if ((TYPE(x) == tc_cons) && 
	(TYPE(car(x)) == tc_symbol) &&
	(streq(":backtrace",get_c_string(car(x)))))
    {
	display_backtrace(x);
	x = NIL;
    }
    else if ((restricted != NIL) &&
	     (restricted_function_call(x) == FALSE))
	err("Expression contains functions not in restricted list",x);
    else
    {
	siod_backtrace = NIL;  /* reset backtrace info */
	if (h->repl_eval == NULL)
	    x = leval(x,NIL);
	else
	    x = (*h->repl_eval)(x);
    }
    if (gc_kind_copying == 1)
      sprintf(tkbuffer,
	      "Evaluation took %g seconds %ld cons work\n",
	      myruntime()-rt,
	      (long)(heap-cw));
    else
      sprintf(tkbuffer,
	      "Evaluation took %g seconds (%g in gc) %ld cons work\n",
	      myruntime()-rt,
	      gc_time_taken,
	      gc_cells_allocated);
    grepl_puts(tkbuffer,h->repl_puts);
    setvar(rintern("!"),x,NIL);  /* save value in var called '!' */
    if (h->repl_print == NULL)
    {
	if (siod_interactive)
	    pprint(x);               /* pretty print the result */
    }
    else
      (*h->repl_print)(x);}
 return(0);}

void set_fatal_exit_hook(void (*fcn)(void))
{fatal_exit_hook = fcn;}

static LISP err(const char *message, LISP x, const char *s)
{
    nointerrupt = 1;
    if NNULLP(x) 
    {
	fprintf(stderr,"SIOD ERROR: %s %s: ",
		(message) ? message : "?",
		(s) ?s : ""
		);
	lprin1f(x,stderr);
	fprintf(stderr,"\n");
	fflush(stderr);
    }
    else
    {
	fprintf(stderr,"SIOD ERROR: %s %s\n",
		(message) ? message : "?",
		(s) ? s : ""
		);
	fflush(stderr);
    }

    if (show_backtrace == 1)
	display_backtrace(NIL);
    
    if (errjmp_ok == 1) {setvar(sym_errobj,x,NIL); longjmp(*est_errjmp,1);}
    close_open_files();  /* can give clue to where error is */
    fprintf(stderr,"%s: fatal error exiting.\n",siod_prog_name);
    if (fatal_exit_hook)
	(*fatal_exit_hook)();
    else
	exit(1);
    return(NIL);
}

LISP err(const char *message, LISP x)
{
  return err(message, x, NULL);
}

LISP err(const char *message, const char *x)
{
  return err(message, NULL, x);
}

LISP errswitch(void)
{return(err("BUG. Reached impossible case",NIL));}

void err_stack(char *ptr)
     /* The user could be given an option to continue here */
{(void)ptr;
 err("the currently assigned stack limit has been exceded",NIL);}

LISP stack_limit(LISP amount,LISP silent)
{if NNULLP(amount)
   {stack_size = get_c_int(amount);
    stack_limit_ptr = STACK_LIMIT(stack_start_ptr,stack_size);}
 if NULLP(silent)
   {sprintf(tkbuffer,"Stack_size = %ld bytes, [%p,%p]\n",
	    stack_size,(void *)stack_start_ptr,(void *)stack_limit_ptr);
    put_st(tkbuffer);
    return(NIL);}
 else
   return(flocons(stack_size));}

const char *get_c_string(LISP x)
{
 if (NULLP(x))
     return "nil";
 else if TYPEP(x,tc_symbol)
   return(PNAME(x));
 else if TYPEP(x,tc_flonum)
 {
     if (FLONMPNAME(x) == NULL)
     {
	 char b[TKBUFFERN];
	 sprintf(b,"%.8g",FLONM(x));
	 FLONMPNAME(x) = (char *)must_malloc(strlen(b)+1);
	 sprintf(FLONMPNAME(x),"%s",b);
     }
     return FLONMPNAME(x);
 }
 else if TYPEP(x,tc_string)
   return(x->storage_as.string.data);
 else
   err("not a symbol or string",x);
 return(NULL);}

LISP lerr(LISP message, LISP x)
{err(get_c_string(message),x);
 return(NIL);}

void gc_fatal_error(void)
{err("ran out of storage",NIL);}

LISP newcell(long type)
{LISP z;
 NEWCELL(z,type);
 return(z);}

LISP flocons(double x)
{LISP z;
 long n=0;
 if ((inums_dim > 0) &&
     ((x - (n = (long)x)) == 0) &&
     (x >= 0) &&
     (n < inums_dim))
   return(inums[n]);
 NEWCELL(z,tc_flonum);
 FLONMPNAME(z) = NULL;
 FLONM(z) = x;
 return(z);}

LISP symcons(char *pname,LISP vcell)
{LISP z;
 NEWCELL(z,tc_symbol);
 PNAME(z) = pname;
 VCELL(z) = vcell;
 return(z);}

char *must_malloc(unsigned long size)
{char *tmp;
 tmp = walloc(char,size);
 if (tmp == (char *)NULL) err("failed to allocate storage from system",NIL);
 return(tmp);}

LISP gen_intern(char *name,int require_copy)
{LISP l,sym,sl;
 const unsigned char *cname;
 long hash=0,n,c,flag;
 flag = no_interrupt(1);
 if (name == NULL)
     return NIL;
 else if (obarray_dim > 1)
   {hash = 0;
    n = obarray_dim;
    cname = (unsigned char *)name;
    while((c = *cname++)) hash = ((hash * 17) ^ c) % n;
    sl = obarray[hash];}
 else
   sl = oblistvar;
 for(l=sl;NNULLP(l);l=CDR(l))
   if (strcmp(name,PNAME(CAR(l))) == 0)
     {no_interrupt(flag);
      return(CAR(l));}
 /* Need a new symbol */
 if (require_copy)
     sym = symcons(wstrdup(name),unbound_marker);
 else
     sym = symcons(name,unbound_marker);
 if (obarray_dim > 1) obarray[hash] = cons(sym,sl);
 oblistvar = cons(sym,oblistvar);
 no_interrupt(flag);
 return(sym);}

LISP cintern(const char *name)
{
    char *dname = (char *)(void *)name;
    return(gen_intern(dname,FALSE));
}

LISP rintern(const char *name)
{
    if (name == 0)
	return NIL;
    char *dname = (char *)(void *)name;
    return gen_intern(dname,TRUE);
}

LISP intern(LISP name)
{return(rintern(get_c_string(name)));}

LISP subrcons(long type, const char *name, SUBR_FUNC f)
{LISP z;
 NEWCELL(z,type);
 (*z).storage_as.subr.name = name;
 (*z).storage_as.subr0.f = f;
 return(z);}

LISP closure(LISP env,LISP code)
{LISP z;
 NEWCELL(z,tc_closure);
 (*z).storage_as.closure.env = env;
 (*z).storage_as.closure.code = code;
 return(z);}

void gc_unprotect(LISP *location)
{
    /* allow LISP values in a location top be gc'ed again */
    struct gc_protected *reg,*l;
    for(l=0,reg = protected_registers; reg; reg = reg->next)
    {
	if (reg->location == location)
	    break;
	l = reg;
    }
    if (reg == 0)
    {
	fprintf(stderr,"Cannot unprotected %lx: never protected\n",
		(unsigned long)*location);
	fflush(stderr);
    }
    else if (l==0) /* its the first one in the list that needs to be deleted */
    {
	reg = protected_registers;
	protected_registers = reg->next;
	wfree(reg);
    }
    else
    {
	reg = l->next;
	l->next = reg->next;
	wfree(reg);
    }

    return;
}

void gc_protect(LISP *location)
{
    struct gc_protected *reg;
    for(reg = protected_registers; reg; reg = reg->next)
    {
	if (reg->location == location)
	    return;   // already protected
    }
    // not protected so add it
    gc_protect_n(location,1);
}

void gc_protect_n(LISP *location,long n)
{struct gc_protected *reg;
 reg = (struct gc_protected *) must_malloc(sizeof(struct gc_protected));
 (*reg).location = location;
 (*reg).length = n;
 (*reg).next = protected_registers;
  protected_registers = reg;}

void gc_protect_sym(LISP *location,const char *st)
{*location = cintern(st);
 gc_protect(location);}

void scan_registers(void)
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
   {location = (*reg).location;
    n = (*reg).length;
    for(j=0;j<n;++j)
      location[j] = gc_relocate(location[j]);}}

static void init_storage_1(int init_heap_size)
{LISP ptr,next,end;
 long j;
 tkbuffer = (char *) must_malloc(TKBUFFERN+1);
 heap_1 = (LISP) must_malloc(sizeof(struct obj)*init_heap_size);
 heap = heap_1;
 which_heap = 1;
 heap_org = heap;
 heap_end = heap + init_heap_size;
 if (gc_kind_copying == 1)
   heap_2 = (LISP) must_malloc(sizeof(struct obj)*init_heap_size);
 else
   {ptr = heap_org;
    end = heap_end;
    while(1)
      {(*ptr).type = tc_free_cell;
       next = ptr + 1;
       if (next < end)
	 {CDR(ptr) = next;
	  ptr = next;}
       else
	 {CDR(ptr) = NIL;
	  break;}}
    freelist = heap_org;}
 gc_protect(&oblistvar);
 gc_protect(&siod_backtrace);
 gc_protect(&current_env);
 if (obarray_dim > 1)
   {obarray = (LISP *) must_malloc(sizeof(LISP) * obarray_dim);
    for(j=0;j<obarray_dim;++j)
      obarray[j] = NIL;
    gc_protect_n(obarray,obarray_dim);}
 unbound_marker = cons(cintern("**unbound-marker**"),NIL);
 gc_protect(&unbound_marker);
 eof_val = cons(cintern("eof"),NIL);
 gc_protect(&eof_val);
 gc_protect(&siod_docstrings);
 gc_protect_sym(&truth,"t");
 setvar(truth,truth,NIL);
 setvar(cintern("nil"),NIL,NIL);
 setvar(cintern("let"),cintern("let-internal-macro"),NIL);
 gc_protect_sym(&sym_errobj,"errobj");
 setvar(sym_errobj,NIL,NIL);
 gc_protect_sym(&sym_quote,"quote");
 gc_protect_sym(&sym_dot,".");
 gc_protect(&open_files);
 if (inums_dim > 0)
   {inums = (LISP *) must_malloc(sizeof(LISP) * inums_dim);
    for(j=0;j<inums_dim;++j)
      {NEWCELL(ptr,tc_flonum);
       FLONM(ptr) = j;
       FLONMPNAME(ptr) = NULL;
       inums[j] = ptr;}
    gc_protect_n(inums,inums_dim);}}
 
void init_storage(int init_heap_size)
{
 init_storage_1(init_heap_size);
 LISP stack_start;
 stack_start_ptr = &stack_start;
 stack_limit_ptr = STACK_LIMIT(stack_start_ptr,stack_size);
}

void init_subr(const char *name, long type, SUBR_FUNC fcn)
{setvar(cintern(name),subrcons(type,name,fcn),NIL);}
void init_subr(const char *name, long type, SUBR_FUNC fcn,const char *doc)
{LISP lname = cintern(name);
 setvar(lname,subrcons(type,name,fcn),NIL);
 setdoc(lname,cstrcons(doc));}

/* New versions requiring documentation strings */
void init_subr_0(const char *name, LISP (*fcn)(void),const char *doc)
{init_subr(name,tc_subr_0,(SUBR_FUNC)fcn,doc);}
void init_subr_1(const char *name, LISP (*fcn)(LISP),const char *doc)
{init_subr(name,tc_subr_1,(SUBR_FUNC)fcn,doc);}
void init_subr_2(const char *name, LISP (*fcn)(LISP,LISP),const char *doc)
{init_subr(name,tc_subr_2,(SUBR_FUNC)fcn,doc);}
void init_subr_3(const char *name, LISP (*fcn)(LISP,LISP,LISP),const char *doc)
{init_subr(name,tc_subr_3,(SUBR_FUNC)fcn,doc);}
void init_subr_4(const char *name, LISP (*fcn)(LISP,LISP,LISP,LISP),const char *doc)
{init_subr(name,tc_subr_4,(SUBR_FUNC)fcn,doc);}
void init_lsubr(const char *name, LISP (*fcn)(LISP),const char *doc)
{init_subr(name,tc_lsubr,(SUBR_FUNC)fcn,doc);}
void init_fsubr(const char *name, LISP (*fcn)(LISP,LISP),const char *doc)
{init_subr(name,tc_fsubr,(SUBR_FUNC)fcn,doc);}
void init_msubr(const char *name, LISP (*fcn)(LISP *,LISP *),const char *doc)
{init_subr(name,tc_msubr,(SUBR_FUNC)fcn,doc);}

struct user_type_hooks *get_user_type_hooks(long type)
{long n;
 if (user_types == NULL)
   {n = sizeof(struct user_type_hooks) * tc_table_dim;
    user_types = (struct user_type_hooks *) must_malloc(n);
    memset(user_types,0,n);}
 if ((type >= 0) && (type < tc_table_dim))
   return(&user_types[type]);
 else
   err("type number out of range",NIL);
 return(NULL);}
 
int siod_register_user_type(const char *name)
{
    // Register a new object type for LISP
    static int siod_user_type = tc_first_user_type;
    int new_type = siod_user_type;
    struct user_type_hooks *th;

    if (new_type == tc_table_dim)
    {
	cerr << "SIOD: no more new types allowed, tc_table_dim needs increased"
	    << endl;
	return tc_table_dim-1;
    }
    else
	siod_user_type++;

    th=get_user_type_hooks(new_type);
    th->name = wstrdup(name);
    return new_type;
}

void set_gc_hooks(long type,
		  int gc_free_once,
		  LISP (*rel)(LISP),
		  LISP (*mark)(LISP),
		  void (*scan)(LISP),
		  void (*free)(LISP),
		  void (*clear)(LISP),
		  long *kind)
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->gc_free_once = gc_free_once;
 p->gc_relocate = rel;
 p->gc_scan = scan;
 p->gc_mark = mark;
 p->gc_free = free;
 p->gc_clear = clear;
 *kind = gc_kind_copying;}

LISP gc_relocate(LISP x)
{LISP nw;
 struct user_type_hooks *p;
 if EQ(x,NIL) return(NIL);
 if ((*x).gc_mark == 1) return(CAR(x));
 switch TYPE(x)
   {case tc_flonum:
       if (FLONMPNAME(x) != NULL)
	   wfree(FLONMPNAME(x));    /* free the print name */
       FLONMPNAME(x) = NULL;
    case tc_cons:
    case tc_symbol:
    case tc_closure:
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_subr_4:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      if ((nw = heap) >= heap_end) gc_fatal_error();
      heap = nw+1;
      memcpy(nw,x,sizeof(struct obj));
      break;
    default:
      p = get_user_type_hooks(TYPE(x));
      if (p->gc_relocate)
	nw = (*p->gc_relocate)(x);
      else
	{if ((nw = heap) >= heap_end) gc_fatal_error();
	 heap = nw+1;
	 memcpy(nw,x,sizeof(struct obj));}}
 (*x).gc_mark = 1;
 CAR(x) = nw;
 return(nw);}

LISP get_newspace(void)
{LISP newspace;
 if (which_heap == 1)
   {newspace = heap_2;
    which_heap = 2;}
 else
   {newspace = heap_1;
    which_heap = 1;}
 heap = newspace;
 heap_org = heap;
 heap_end = heap + heap_size;
 return(newspace);}

void scan_newspace(LISP newspace)
{LISP ptr;
 struct user_type_hooks *p;
 for(ptr=newspace; ptr < heap; ++ptr)
   {switch TYPE(ptr)
      {case tc_cons:
       case tc_closure:
	 CAR(ptr) = gc_relocate(CAR(ptr));
	 CDR(ptr) = gc_relocate(CDR(ptr));
	 break;
       case tc_symbol:
	 VCELL(ptr) = gc_relocate(VCELL(ptr));
	 break;
       case tc_flonum:
       case tc_subr_0:
       case tc_subr_1:
       case tc_subr_2:
       case tc_subr_3:
       case tc_subr_4:
       case tc_lsubr:
       case tc_fsubr:
       case tc_msubr:
	 break;
       default:
	 p = get_user_type_hooks(TYPE(ptr));
	 if (p->gc_scan) (*p->gc_scan)(ptr);}}}

void free_oldspace(LISP space,LISP end)
{LISP ptr;
 struct user_type_hooks *p;
 for(ptr=space; ptr < end; ++ptr)
   if (ptr->gc_mark == 0)
     switch TYPE(ptr)
       {case tc_cons:
	case tc_closure:
	case tc_symbol:
	   break;
	case tc_flonum:
	   if (FLONMPNAME(ptr) != NULL)
	       wfree(FLONMPNAME(ptr));    /* free the print name */
	   FLONMPNAME(ptr) = NULL;
	   break;
        case tc_string:
          wfree(ptr->storage_as.string.data);
	  break;
	case tc_subr_0:
	case tc_subr_1:
	case tc_subr_2:
	case tc_subr_3:
	case tc_subr_4:
	case tc_lsubr:
	case tc_fsubr:
	case tc_msubr:
	  break;
	default:
	  p = get_user_type_hooks(TYPE(ptr));
	  if (p->gc_free) 
	    (*p->gc_free)(ptr);
       }
}
      
void gc_stop_and_copy(void)
{LISP newspace,oldspace,end;
 long flag;
 int ej_ok;
 flag = no_interrupt(1);
 fprintf(stderr,"GC ing \n");
 ej_ok = errjmp_ok;
 errjmp_ok = 0;
 oldspace = heap_org;
 end = heap;
 old_heap_used = end - oldspace;
 newspace = get_newspace();
 scan_registers();
 scan_newspace(newspace);
 free_oldspace(oldspace,end);
 errjmp_ok = ej_ok;
 no_interrupt(flag);}

void gc_for_newcell(void)
{long flag;
 int ej_ok;
/* if (errjmp_ok == 0) gc_fatal_error(); */
 flag = no_interrupt(1);
 ej_ok = errjmp_ok;
 errjmp_ok = 0;
 gc_mark_and_sweep();
 errjmp_ok = ej_ok;
 no_interrupt(flag);
 if NULLP(freelist) gc_fatal_error();}

static void gc_mark_and_sweep(void)
{LISP stack_end;
 gc_ms_stats_start();
 setjmp(save_regs_gc_mark);
 mark_locations((LISP *) save_regs_gc_mark,
                (LISP *) (((char *) save_regs_gc_mark) + sizeof(save_regs_gc_mark)));
 mark_protected_registers();
 mark_locations((LISP *) stack_start_ptr,
		(LISP *) &stack_end);
#ifdef THINK_C
 mark_locations((LISP *) ((char *) stack_start_ptr + 2),
		(LISP *) ((char *) &stack_end + 2));
#endif
 gc_sweep();
 gc_ms_stats_end();}

static void gc_ms_stats_start(void)
{gc_rt = myruntime();
 gc_cells_collected = 0;
 if (gc_status_flag)
     fprintf(stderr,"[starting GC]\n");}

static void gc_ms_stats_end(void)
{gc_rt = myruntime() - gc_rt;
 gc_time_taken = gc_time_taken + gc_rt;
 if (gc_status_flag)
     fprintf(stderr,"[GC took %g cpu seconds, %ld cells collected]\n",
	     gc_rt,
	     gc_cells_collected);}

void gc_mark(LISP ptr)
{struct user_type_hooks *p;

 gc_mark_loop:
 if NULLP(ptr) return;
 if ((*ptr).gc_mark) return;
 (*ptr).gc_mark = 1;
 switch ((*ptr).type)
   {case tc_flonum:
      break;
    case tc_cons:
      gc_mark(CAR(ptr));
      ptr = CDR(ptr);
      goto gc_mark_loop;
    case tc_symbol:
      ptr = VCELL(ptr);
      goto gc_mark_loop;
    case tc_closure:
      gc_mark((*ptr).storage_as.closure.code);
      ptr = (*ptr).storage_as.closure.env;
      goto gc_mark_loop;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_subr_4:
      break;
    case tc_string:
      break;
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      break;
    default:
      p = get_user_type_hooks(TYPE(ptr));
      if (p->gc_mark)
	ptr = (*p->gc_mark)(ptr);}}

static void mark_protected_registers(void)
{struct gc_protected *reg;
 LISP *location;
 long j,n;
 for(reg = protected_registers; reg; reg = (*reg).next)
 {
     location = (*reg).location;
     n = (*reg).length; 
     for(j=0;j<n;++j)
	 gc_mark(location[j]);}}

static void mark_locations(LISP *start,LISP *end)
{LISP *tmp;
 long n;
 if (start > end)
   {tmp = start;
    start = end;
    end = tmp;}
 n = end - start;
 mark_locations_array(start,n);}

static void mark_locations_array(LISP *x,long n)
{int j;
 LISP p;
 for(j=0;j<n;++j)
   {p = x[j];
    if ((p >= heap_org) &&
	(p < heap_end) &&
	(((((char *)p) - ((char *)heap_org)) % sizeof(struct obj)) == 0) &&
	NTYPEP(p,tc_free_cell))
      gc_mark(p);}}

static void gc_sweep(void)
{LISP ptr,end,nfreelist;
 long n;
 struct user_type_hooks *p;
 end = heap_end;
 n = 0;
 nfreelist = NIL;
 start_rememberring_dead();
 for(ptr=heap_org; ptr < end; ++ptr)
     if (((*ptr).gc_mark) == 0)
     {switch((*ptr).type)
	{case tc_flonum:
	    if (FLONMPNAME(ptr) != NULL)
		wfree(FLONMPNAME(ptr));    /* free the print name */
	    FLONMPNAME(ptr) = NULL;
	    break;
         case tc_string:
	    wfree(ptr->storage_as.string.data);
	    break;
	 case tc_free_cell:
	 case tc_cons:
	 case tc_closure:
	 case tc_symbol:
	 case tc_subr_0:
	 case tc_subr_1:
	 case tc_subr_2:
	 case tc_subr_3:
	 case tc_subr_4:
	 case tc_lsubr:
	 case tc_fsubr:
	 case tc_msubr:
	   break;
	 default:
	   p = get_user_type_hooks(TYPE(ptr));
	   if (p->gc_free)
	     {
	     if (p->gc_free_once)
	       {
		 if (!is_dead(USERVAL(ptr)))
		   {
		     (*p->gc_free)(ptr);
		     mark_as_dead(USERVAL(ptr));
		   }
	       }
	     else
	       (*p->gc_free)(ptr);
	     }
	}
      ++n;
      (*ptr).type = tc_free_cell;
      CDR(ptr) = nfreelist;
      nfreelist = ptr;
     }
   else
     {
     (*ptr).gc_mark = 0;
     p = get_user_type_hooks(TYPE(ptr));
     if (p->gc_clear)
       (*p->gc_clear)(ptr);
     }
 gc_cells_collected = n;
 freelist = nfreelist;
}

LISP user_gc(LISP args)
{long old_status_flag,flag;
 int ej_ok;
 if (gc_kind_copying == 1)
   err("implementation cannot GC at will with stop-and-copy\n",
       NIL);
 flag = no_interrupt(1);
 ej_ok = errjmp_ok;
 errjmp_ok = 0;
 old_status_flag = gc_status_flag;
 if NNULLP(args)
 {
   if NULLP(car(args)) 
       gc_status_flag = 0; 
   else 
       gc_status_flag = 1;
 }
 gc_mark_and_sweep();
 gc_status_flag = old_status_flag;
 errjmp_ok = ej_ok;
 no_interrupt(flag);

 return(NIL);}

LISP set_backtrace(LISP n)
{
  if (n)
      show_backtrace = 1;
  else
      show_backtrace = 0;
  return n;
}
      
LISP gc_status(LISP args)
{LISP l;
 int n;
 if NNULLP(args) 
 {
   if NULLP(car(args)) gc_status_flag = 0; else gc_status_flag = 1;
 }
 if (gc_kind_copying == 1)
   {if (gc_status_flag)
      fput_st(fwarn,"garbage collection is on\n");
   else
     fput_st(fwarn,"garbage collection is off\n");
    sprintf(tkbuffer,"%ld allocated %ld free\n",
	    (long)(heap - heap_org),(long)(heap_end - heap));
    fput_st(fwarn,tkbuffer);}
 else
   {if (gc_status_flag)
      fput_st(fwarn,"garbage collection verbose\n");
    else
      fput_st(fwarn,"garbage collection silent\n");
    {for(n=0,l=freelist;NNULLP(l); ++n) l = CDR(l);
     sprintf(tkbuffer,"%ld allocated %ld free\n",
	     (long)((heap_end - heap_org) - n),(long)n);
     fput_st(fwarn,tkbuffer);}}
 return(NIL);}

LISP leval_args(LISP l,LISP env)
{LISP result,v1,v2,tmp;
 if NULLP(l) return(NIL);
 if NCONSP(l) err("bad syntax argument list",l);
 result = cons(leval(CAR(l),env),NIL);
 for(v1=result,v2=CDR(l);
     CONSP(v2);
     v1 = tmp, v2 = CDR(v2))
  {tmp = cons(leval(CAR(v2),env),NIL);
   CDR(v1) = tmp;}
 if NNULLP(v2) err("bad syntax argument list",l);
 return(result);}

LISP extend_env(LISP actuals,LISP formals,LISP env)
{
    if SYMBOLP(formals)
        return(cons(cons(cons(formals,NIL),cons(actuals,NIL)),env));
    else
        return(cons(cons(formals,actuals),env));
}

#define ENVLOOKUP_TRICK 1
LISP global_var = NIL;
LISP global_env = NIL;

LISP envlookup(LISP var,LISP env)
{LISP frame,al,fl,tmp;
    global_var = var;
    global_env = env;
 for(frame=env;CONSP(frame);frame=CDR(frame))
   {tmp = CAR(frame);
    if NCONSP(tmp) err("damaged frame",tmp);
    for(fl=CAR(tmp),al=CDR(tmp);CONSP(fl);fl=CDR(fl),al=CDR(al))
      {if NCONSP(al) err("too few arguments",tmp);
       if EQ(CAR(fl),var) return(al);}
    /* suggested by a user. It works for reference (although conses)
       but doesn't allow for set! to work properly... */
#if (ENVLOOKUP_TRICK)
    if (SYMBOLP(fl) && EQ(fl, var)) return(cons(al, NIL));
#endif
  }
 if NNULLP(frame) 
              err("damaged env",env);
 return(NIL);}

void set_eval_hooks(long type,LISP (*fcn)(LISP, LISP *,LISP *))
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->leval = fcn;}

LISP leval(LISP x,LISP qenv)
{LISP tmp,arg1,rval;
    LISP env;
 struct user_type_hooks *p;
 env = qenv;
 STACK_CHECK(&x);
 siod_backtrace = cons(x,siod_backtrace);
 loop:
 INTERRUPT_CHECK();
 current_env = env;
 switch TYPE(x)
   {case tc_symbol:
      tmp = envlookup(x,env);
      if NNULLP(tmp) 
      {
	  siod_backtrace = cdr(siod_backtrace);
	  return(CAR(tmp));
      }
      tmp = VCELL(x);
      if EQ(tmp,unbound_marker) err("unbound variable",x);
      siod_backtrace = cdr(siod_backtrace);
      return tmp;
    case tc_cons:
      tmp = CAR(x);
      switch TYPE(tmp)
	{case tc_symbol:
	   tmp = envlookup(tmp,env);
	   if NNULLP(tmp)
	     {tmp = CAR(tmp);
	      break;}
	   tmp = VCELL(CAR(x));
	   if EQ(tmp,unbound_marker) err("unbound variable",CAR(x));
	   break;
	 case tc_cons:
	   tmp = leval(tmp,env);
	   break;}
      switch TYPE(tmp)
	{case tc_subr_0:
	    rval = SUBR0(tmp)();
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_subr_1:
	    rval = SUBR1(tmp)(leval(car(CDR(x)),env)); 
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_subr_2:
	    x = CDR(x);
	    arg1 = leval(car(x),env);
	    x = NULLP(x) ? NIL : CDR(x);
	    rval = SUBR2(tmp)(arg1,leval(car(x),env));
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_subr_3:
	    x = CDR(x);
	    arg1 = leval(car(x),env);
	    x = NULLP(x) ? NIL : CDR(x);
	    rval = SUBR3(tmp)(arg1,leval(car(x),env),leval(car(cdr(x)),env));
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_subr_4:
	    x = CDR(x);
	    arg1 = leval(car(x),env);
	    x = NULLP(x) ? NIL : CDR(x);
	    rval = SUBR4(tmp)(arg1,leval(car(x),env),
			      leval(car(cdr(x)),env),
			      leval(car(cdr(cdr(x))),env));
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_lsubr:
	    rval = SUBR1(tmp)(leval_args(CDR(x),env));
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_fsubr:
	    rval = SUBR2(tmp)(CDR(x),env);
	    siod_backtrace = cdr(siod_backtrace);
	    return rval;
	 case tc_msubr:
	   if NULLP(SUBRM(tmp)(&x,&env)) 
	   {
	       siod_backtrace = cdr(siod_backtrace);
	       return(x);
	   }
	   goto loop;
	 case tc_closure:
           env = extend_env(leval_args(CDR(x),env),
			    car((*tmp).storage_as.closure.code),
			    (*tmp).storage_as.closure.env);
	   x = cdr((*tmp).storage_as.closure.code);
	   goto loop;
	 case tc_symbol:
	   x = cons(tmp,cons(cons(sym_quote,cons(x,NIL)),NIL));
	   x = leval(x,NIL);
	   goto loop;
	 default:
	   p = get_user_type_hooks(TYPE(tmp));
	   if (p->leval)
	     {if NULLP((*p->leval)(tmp,&x,&env)) 
	      {
		  siod_backtrace = cdr(siod_backtrace);
		  return(x); 
	      }
	     else 
		 goto loop;}
	   err("bad function",tmp);}
    default:
        siod_backtrace = cdr(siod_backtrace);
        return(x);}}

void set_print_hooks(long type,
		     void (*prin1)(LISP, FILE *),
		     void (*print_string)(LISP, char *)
		     )
{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->prin1 = prin1;
 p->print_string = print_string;
}

void set_io_hooks(long type,
		  LISP (*fast_print)(LISP,LISP),
		  LISP (*fast_read)(int,LISP))

{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->fast_print = fast_print;
 p->fast_read = fast_read;
}

void set_type_hooks(long type,
		    long (*c_sxhash)(LISP,long),
		    LISP (*equal)(LISP,LISP))


{struct user_type_hooks *p;
 p = get_user_type_hooks(type);
 p->c_sxhash = c_sxhash;
 p->equal = equal;
}

int f_getc(FILE *f)
{long iflag;
 int c;
 iflag = no_interrupt(1);
 c = getc(f);
 if ((c == '\n') && (f == stdin) && (siod_interactive))
 {
     fprintf(stdout,"%s",repl_prompt);
     fflush(stdout);
 }
 no_interrupt(iflag);
 return(c);}

void f_ungetc(int c, FILE *f)
{ungetc(c,f);}

#ifdef WIN32
int winsock_unget_buffer;
bool winsock_unget_buffer_unused=true;
bool use_winsock_unget_buffer;

int f_getc_winsock(HANDLE h)
{long iflag,dflag;
 char c;
 DWORD lpNumberOfBytesRead;
 iflag = no_interrupt(1);
 if (use_winsock_unget_buffer)
 {
	use_winsock_unget_buffer = false;
	return winsock_unget_buffer;
 }

 if (SOCKET_ERROR == recv((SOCKET)h,&c,1,0))
 {
    if (WSAECONNRESET == GetLastError()) // The connection was closed.
        c=EOF;
    else
        cerr << "f_getc_winsock(): error reading from socket\n";
 }

 winsock_unget_buffer=c;
 winsock_unget_buffer_unused = false;

 no_interrupt(iflag);
 return(c);}

void f_ungetc_winsock(int c, HANDLE h)
{
 if (winsock_unget_buffer_unused)
 {
  cerr << "f_ungetc_winsock: tried to unget before reading socket\n";
 }
use_winsock_unget_buffer = true;}
#endif

int flush_ws(struct gen_readio *f,const char *eoferr)
{int c,commentp;
 commentp = 0;
 while(1)
   {c = GETC_FCN(f);
    if (c == EOF) { if (eoferr) err(eoferr,NIL); else return(c); }
    if (commentp) {if (c == '\n') commentp = 0;}
    else if (c == ';') commentp = 1;
    else if (!isspace(c)) return(c);}}

LISP lreadf(FILE *f)
{struct gen_readio s;
 if ((f == stdin) && (isatty(0)) && (siod_interactive))
 {   /* readline (if selected) stuff -- only works with a terminal */
     s.getc_fcn = (int (*)(char *))siod_fancy_getc;
     s.ungetc_fcn = (void (*)(int, char *))siod_fancy_ungetc;
     s.cb_argument = (char *) f;
 }
 else  /* normal stuff */
 {
     s.getc_fcn = (int (*)(char *))f_getc;
     s.ungetc_fcn = (void (*)(int, char *))f_ungetc;
     s.cb_argument = (char *) f;
 }
 return(readtl(&s));}

#ifdef WIN32
LISP lreadwinsock(void)
{
	struct gen_readio s;
	s.getc_fcn = (int (*)(char *))f_getc_winsock;
	s.ungetc_fcn = (void (*)(int, char *))f_ungetc_winsock;
	s.cb_argument = (char *) siod_server_socket;
	return(readtl(&s));}
#endif

LISP readtl(struct gen_readio *f)
{int c;
 c = flush_ws(f,(char *)NULL);
 if (c == EOF) return(eof_val);
 UNGETC_FCN(c,f);
 return(lreadr(f));}

void set_read_hooks(char *all_set,char *end_set,
		    LISP (*fcn1)(int, struct gen_readio *),
		    LISP (*fcn2)(char *,long, int *))
{user_ch_readm = all_set;
 user_te_readm = end_set;
 user_readm = fcn1;
 user_readt = fcn2;}

static LISP lreadr(struct gen_readio *f)
{int c,j;
 char *p;
 const char *pp, *last_prompt;
 LISP rval;
 STACK_CHECK(&f);
 p = tkbuffer;
 c = flush_ws(f,"end of file inside read");
 switch (c)
   {case '(':
       last_prompt = repl_prompt;
       repl_prompt = siod_secondary_prompt;
       rval = lreadparen(f);
       repl_prompt = last_prompt;
       return rval;
    case ')':
      err("unexpected close paren",NIL);
    case '\'':
      return(cons(sym_quote,cons(lreadr(f),NIL)));
    case '`':
      return(cons(cintern("+internal-backquote"),lreadr(f)));
    case ',':
      c = GETC_FCN(f);
      switch(c)
	{case '@':
	   pp = "+internal-comma-atsign";
	   break;
	 case '.':
	   pp = "+internal-comma-dot";
	   break;
	 default:
	   pp = "+internal-comma";
	   UNGETC_FCN(c,f);}
      return(cons(cintern(pp),lreadr(f)));
    case '"':
       last_prompt = repl_prompt;
       repl_prompt = siod_secondary_prompt;
       rval = lreadstring(f);
       repl_prompt = last_prompt;
       return rval;
    default:
      if ((user_readm != NULL) && strchr(user_ch_readm,c))
	return((*user_readm)(c,f));}
 *p++ = c;
 for(j = 1; j<TKBUFFERN; ++j)
   {c = GETC_FCN(f);
    if (c == EOF) return(lreadtk(j));
    if (isspace(c)) return(lreadtk(j));
    if (strchr("()'`,;\"",c) || strchr(user_te_readm,c))
      {UNGETC_FCN(c,f);return(lreadtk(j));}
    *p++ = c;}
 return(err("symbol larger than maxsize (can you use a string instead?)",NIL));}

#if 0
LISP lreadparen(struct gen_readio *f)
{int c;
 LISP tmp;
 c = flush_ws(f,"end of file inside list");
 if (c == ')') return(NIL);
 UNGETC_FCN(c,f);
 tmp = lreadr(f);
 if EQ(tmp,sym_dot)
   {tmp = lreadr(f);
    c = flush_ws(f,"end of file inside list");
    if (c != ')') err("missing close paren",NIL);
    return(tmp);}
 return(cons(tmp,lreadparen(f)));}
#endif

/* Iterative version of the above */
static LISP lreadparen(struct gen_readio *f)
{
    int c;
    LISP tmp,l=NIL;
    LISP last=l;

    while ((c = flush_ws(f,"end of file inside list")) != ')')
    {
	UNGETC_FCN(c,f);
	tmp = lreadr(f);
	if EQ(tmp,sym_dot)
	{
	    tmp = lreadr(f);
	    c = flush_ws(f,"end of file inside list");
	    if (c != ')') err("missing close paren",NIL);
	    if (l == NIL) err("no car for dotted pair",NIL);
	    CDR(last) = tmp;
	    break;
	}
	if (l == NIL)
	{
	    l = cons(tmp,NIL);
	    last = l;
	}
	else
	{
	    CDR(last) = cons(tmp,NIL);
	    last = cdr(last);
	}
    }
    return l;
}

static LISP lreadstring(struct gen_readio *f)
{
    int j,c,n;
    static int len=TKBUFFERN;
    static char *str = 0;
    char *q;
    LISP qq;
    j = 0;
    if (str == 0)
	str = (char *)must_malloc(len * sizeof(char));
    while(((c = GETC_FCN(f)) != '"') && (c != EOF))
    {
	if (c == '\\')
	{c = GETC_FCN(f);
	 if (c == EOF) err("eof after \\",NIL);
	 switch(c)
	 {case 'n':
	     c = '\n';
	     break;
	   case 't':
	     c = '\t';
	     break;
	   case 'r':
	     c = '\r';
	     break;
	   case 'd':
	     c = 0x04;
	     break;
	   case 'N':
	     c = 0;
	     break;
	   case 's':
	     c = ' ';
	     break;
	   case '0':
	     n = 0;
	     while(1)
	     {c = GETC_FCN(f);
	      if (c == EOF) err("eof after \\0",NIL);
	      if (isdigit(c))
		  n = n * 8 + c - '0';
	      else
	      {UNGETC_FCN(c,f);
	       break;}}
	     c = n;}}
	if ((j + 1) >= len) 
	{
	    /* EST_String full so double the buffer, copy and continue */
	    q = (char *)must_malloc(len*2*sizeof(char));
	    strncpy(q,str,len);
	    wfree(str);
	    str = q;
	    len = len*2;
	}
	str[j] = c;
	++j;
    }
    str[j] = 0;
    qq = strcons(j,str);
    return qq;
}

LISP lreadtk(long j)
{int flag;
 unsigned char *p;
 LISP tmp;
 int adigit;
 p = (unsigned char *)tkbuffer;
 p[j] = 0;
 if (user_readt != NULL)
   {tmp = (*user_readt)((char *)p,j,&flag);
    if (flag) return(tmp);}
 if (strcmp("nil",tkbuffer) == 0)
     return NIL;
 if (*p == '-') p+=1;
 adigit = 0;
 while((*p < 128) && (isdigit(*p))) {p+=1; adigit=1;}
 if (*p=='.')
   {p += 1;
    while((*p < 128) && (isdigit(*p))) {p+=1; adigit=1;}}
 if (!adigit) goto a_symbol;
 if (*p=='e')
   {p+=1;
    if (*p=='-'||*p=='+') p+=1;
    if ((!isdigit(*p) || (*p > 127))) goto a_symbol; else p+=1;
    while((*p < 128) && (isdigit(*p))) p+=1;}
 if (*p) goto a_symbol;
 return(flocons(atof(tkbuffer)));
 a_symbol:
 return(rintern(tkbuffer));}
      
LISP siod_quit(void)
{open_files = NIL;  // will be closed on exit with no warnings
 if (errjmp_ok) longjmp(*est_errjmp,2);
 else exit(0);
 return(NIL);}

LISP l_exit(LISP arg)
{
    if (arg == NIL)
	exit(0);
    else
	exit((int)FLONM(arg));

    // never happens
    return NULL;
}

LISP lfwarning(LISP mode)
{
    /* if mode is non-nil switch warnings on */
    if (mode == NIL)
	fwarn = NULL;
    else
	fwarn = stdout;
    return NIL;
}

LISP closure_code(LISP exp)
{return(exp->storage_as.closure.code);}

LISP closure_env(LISP exp)
{return(exp->storage_as.closure.env);}

int get_c_int(LISP x)
{if NFLONUMP(x) err("not a number",x);
 return((int)FLONM(x));}

double get_c_double(LISP x)
{if NFLONUMP(x) err("not a number",x);
 return(FLONM(x));}

float get_c_float(LISP x)
{if NFLONUMP(x) err("not a number",x);
 return((float)FLONM(x));}


void init_subrs_base(void)
{
 init_subr_2("eval",leval,
 "(eval DATA)\n\
  Evaluate DATA and return result.");
 init_lsubr("gc-status",gc_status,
 "(gc-status OPTION)\n\
  Control summary information during garbage collection.  If OPTION is t,\n\
  output information at each garbage collection, if nil do gc silently.");
 init_lsubr("gc",user_gc,
 "(gc)\n\
  Collect garbage now, where gc method supports it.");
 init_subr_2("error",lerr,
 "(error MESSAGE DATA)\n\
  Prints MESSAGE about DATA and throws an error.");
 init_subr_0("quit",siod_quit,
 "(quit)\n\
  Exit from program, does not return.");
 init_subr_1("exit",l_exit,
 "(exit [RCODE])\n\
  Exit from program, if RCODE is given it is given as an argument to\n\
  the system call exit.");
 init_subr_2("env-lookup",envlookup,
 "(env-lookup VARNAME ENVIRONMENT)\n\
  Return value of VARNAME in ENVIRONMENT.");
 init_subr_1("fwarning",lfwarning,
 "(fwarning MODE)\n\
  For controlling various levels of warning messages.  If MODE is nil, or\n\
  not specified stop all warning messages from being displayed.  If MODE\n\
  display warning messages.");
 init_subr_2("%%stack-limit",stack_limit,
 "(%%stack-limit AMOUNT SILENT)\n\
  Set stacksize to AMOUNT, if SILENT is non nil do it silently.");
 init_subr_1("intern",intern,
 "(intern ATOM)\n\
  Intern ATOM on the oblist.");
 init_subr_2("%%closure",closure,
 "(%%closure ENVIRONMENT CODE)\n\
  Make a closure from given environment and code.");
 init_subr_1("%%closure-code",closure_code,
 "(%%closure-code CLOSURE)\n\
  Return code part of closure.");
 init_subr_1("%%closure-env",closure_env,
 "(%%closure-env CLOSURE)\n\
  Return environment part of closure.");
 init_subr_1("set_backtrace",set_backtrace,
 "(set_backtrace arg)\n\
  If arg is non-nil a backtrace will be display automatically after errors\n\
  if arg is nil, a backtrace will not automatically be displayed (use\n\
  (:backtrace) for display explicitly.");
 init_subr_1("set_server_safe_functions",set_restricted,
 "(set_server_safe_functions LIST)\n\
 Sets restricted list to LIST.  When restricted list is non-nil only\n\
 functions whose names appear in this list may be executed.  This\n\
 is used so that clients in server mode may be restricted to a small\n\
 number of safe commands.  [see Server/client API]");

}

void init_subrs(void)
{
  init_subrs_base();
  init_subrs_core();
  init_subrs_doc();
  init_subrs_file();
  init_subrs_format();
  init_subrs_list();
  init_subrs_math();
  init_subrs_str();
  init_subrs_sys();
  init_subrs_xtr();  // arrays and hash tables 
}

/* err0,pr,prp are convenient to call from the C-language debugger */

void err0(void)
{err("0",NIL);}

void pr(LISP p)
{if ((p >= heap_org) &&
     (p < heap_end) &&
     (((((char *)p) - ((char *)heap_org)) % sizeof(struct obj)) == 0))
   pprint(p);
 else
   put_st("invalid\n");}

void prp(LISP *p)
{if (!p) return;
 pr(*p);}

LISP siod_make_typed_cell(long type, void *s)
{
    LISP ptr;

    NEWCELL(ptr,type);
    USERVAL(ptr) = s;

    return ptr;
}

static LISP set_restricted(LISP l)
{
    // Set restricted list
    
    if (restricted == NIL)
	gc_protect(&restricted);
    
    restricted = l;
    return NIL;
}

static int restricted_function_call(LISP l)
{
    // Checks l recursively to ensure all function calls
    // are in the restricted list
    LISP p;
    
    if (l == NIL)
	return TRUE;
    else if (!consp(l))
	return TRUE;
    else if (TYPE(car(l)) == tc_symbol)
    {
	if (streq("quote",get_c_string(car(l))))
	    return TRUE;
	else if (siod_member_str(get_c_string(car(l)),restricted) == NIL)
	    return FALSE;
    }
    else if (restricted_function_call(car(l)) == FALSE)
	return FALSE;

    // As its some type of list with a valid car, check the cdr
    for (p=cdr(l); consp(p); p=cdr(p))
	if (restricted_function_call(car(p)) == FALSE)
	    return FALSE;
    return TRUE;
}

