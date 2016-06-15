/* Scheme In One Defun, but in C this time.

 *                        COPYRIGHT (c) 1988-1992 BY                        *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

Declarations which are private to SLIB.C internals.

*/

#ifndef __SIODP_H__
#define __SIODP_H__

#include "io.h"

typedef int (*repl_getc_fn)(FILE *);
typedef void (*repl_ungetc_fn)(int,FILE *);

/* Will get to editline functions if supported */
extern repl_getc_fn siod_fancy_getc;
extern repl_ungetc_fn siod_fancy_ungetc;
extern "C" const char *repl_prompt;

extern char *tkbuffer;
extern LISP heap,heap_end,heap_org;
extern LISP oblistvar;
extern LISP open_files;
extern LISP eof_val;
extern LISP siod_docstrings;
extern int siod_interactive;
void sock_acknowledge_error();
extern FILE *fwarn;
extern LISP unbound_marker;
extern long gc_kind_copying;
extern LISP freelist;
extern long gc_cells_allocated;
extern int siod_server_socket;
extern "C" int rl_pos;

struct user_type_hooks
{
 char *name;
 char gc_free_once;
 LISP (*gc_relocate)(LISP);
 void (*gc_scan)(LISP);
 LISP (*gc_mark)(LISP);
 void (*gc_free)(LISP);
 void (*gc_clear)(LISP);
 void (*prin1)(LISP, FILE *);
 void (*print_string)(LISP, char *);
 LISP (*leval)(LISP, LISP *, LISP *);
 long (*c_sxhash)(LISP,long);
 LISP (*fast_print)(LISP,LISP);
 LISP (*fast_read)(int,LISP);
 LISP (*equal)(LISP,LISP);};

struct catch_frame
{LISP tag;
 LISP retval;
 jmp_buf cframe;
 struct catch_frame *next;};

struct gc_protected
{LISP *location;
 long length;
 struct gc_protected *next;};

#define NEWCELL(_into,_type)          \
{if (gc_kind_copying == 1)            \
   {if ((_into = heap) >= heap_end)   \
      gc_fatal_error();               \
    heap = _into+1;}                  \
 else                                 \
   {if NULLP(freelist)                \
      gc_for_newcell();               \
    _into = freelist;                 \
    freelist = CDR(freelist);         \
    ++gc_cells_allocated;}            \
 (*_into).gc_mark = 0;                \
 (*_into).type = (short) _type;}
#if 0
#define NEWCELL(_into,_type) NNEWCELL(&_into,_type)
void NNEWCELL (LISP *_into,long _type);
#endif

#ifdef THINK_C
extern int ipoll_counter;
void full_interrupt_poll(int *counter);
#define INTERRUPT_CHECK() if (--ipoll_counter < 0) full_interrupt_poll(&ipoll_counter)
#else
#define INTERRUPT_CHECK()
#endif

extern char *stack_limit_ptr;

#define STACK_LIMIT(_ptr,_amt) (((char *)_ptr) - (_amt))

/* This is wrong if stack grows in different direction */
#define STACK_CHECK(_ptr) \
  if (((char *) (_ptr)) < stack_limit_ptr) err_stack((char *) _ptr);

#define TKBUFFERN 256

void err_stack(char *);

#if defined(VMS) && defined(VAX)
#define SIG_restargs ,...
#else
#define SIG_restargs
#endif

void init_storage(int init_heap_size);
void init_subrs_base(void);
void init_subrs_core(void);
void init_subrs_doc(void);
void init_subrs_file(void);
void init_subrs_format(void);
void init_subrs_list(void);
void init_subrs_math(void);
void init_subrs_sys(void);
void init_subrs_srv(void);
void init_subrs_str(void);
void init_subrs_xtr(void);

void need_n_cells(int n);

char *must_malloc(unsigned long size);

LISP gc_relocate(LISP x);
void gc_fatal_error(void);
void gc_for_newcell(void);
struct user_type_hooks *get_user_type_hooks(long type);
void gc_mark(LISP ptr);
LISP newcell(long type);

void put_st(const char *st);
int f_getc(FILE *f);
void f_ungetc(int c, FILE *f);
long no_interrupt(long n);
LISP readtl(struct gen_readio *f);
long repl_driver(long want_sigint,long want_init,struct repl_hooks *);

LISP leval_args(LISP l,LISP env);
LISP extend_env(LISP actuals,LISP formals,LISP env);
LISP envlookup(LISP var,LISP env);
LISP closure(LISP env,LISP code);
extern struct catch_frame *catch_framep;
void close_open_files(void);
void close_open_files_upto(LISP end);
void pprintf(FILE *fd,LISP exp,int indent,int width, int depth,int length);

#if 0
void handle_sigfpe(int sig SIG_restargs);
void handle_sigint(int sig SIG_restargs);
void err_ctrl_c(void);
double myruntime(void);
void grepl_puts(char *,void (*)(char *));
LISP gen_intern(char *name,int freeable);
void scan_registers(void);
void init_storage_1(int heap_size);
LISP get_newspace(void);
void scan_newspace(LISP newspace);
void free_oldspace(LISP space,LISP end);
void gc_stop_and_copy(void);
void gc_mark_and_sweep(void);
void gc_ms_stats_start(void);
void gc_ms_stats_end(void);
void mark_protected_registers(void);
void mark_locations(LISP *start,LISP *end);
void mark_locations_array(LISP *x,long n);
void gc_sweep(void);
LISP leval_setq(LISP args,LISP env);
LISP syntax_define(LISP args);
LISP leval_define(LISP args,LISP env);
LISP leval_if(LISP *pform,LISP *penv);
LISP leval_lambda(LISP args,LISP env);
LISP leval_progn(LISP *pform,LISP *penv);
LISP leval_or(LISP *pform,LISP *penv);
LISP leval_and(LISP *pform,LISP *penv);
LISP leval_catch(LISP args,LISP env);
LISP lthrow(LISP tag,LISP value);
LISP leval_let(LISP *pform,LISP *penv);
LISP reverse(LISP l);
LISP let_macro(LISP form);
LISP leval_quote(LISP args,LISP env);
LISP leval_tenv(LISP args,LISP env);
int flush_ws(struct gen_readio *f,const char *eoferr);
LISP lreadr(struct gen_readio *f);
LISP lreadparen(struct gen_readio *f);
LISP arglchk(LISP x);

int rfs_getc(unsigned char **p);
void rfs_ungetc(unsigned char c,unsigned char **p);
LISP lreadstring(struct gen_readio *f);

void file_gc_free(LISP ptr);
void file_prin1(LISP ptr,FILE *f);
LISP fd_to_scheme_file(int fd,
		       const char *name, const char *how, 
		       int close_on_error);
LISP lgetc(LISP p);
LISP lputc(LISP c,LISP p);
LISP lputs(LISP str,LISP p);

LISP lftell(LISP file);
LISP lfseek(LISP file,LISP offset,LISP direction);
LISP lfread(LISP size,LISP file);
LISP lfwrite(LISP string,LISP file);


LISP leval_while(LISP args,LISP env);

void init_subrs_a(void);
void init_subrs_1(void);

LISP stack_limit(LISP,LISP);

void err0(void);
void pr(LISP);
void prp(LISP *);

LISP closure_code(LISP exp);
LISP closure_env(LISP exp);
LISP lwhile(LISP form,LISP env);

LISP siod_send_lisp_to_client(LISP x);
#endif


#endif
