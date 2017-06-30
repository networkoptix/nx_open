/*  
 *                   COPYRIGHT (c) 1988-1994 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

 * Reorganization of files (Mar 1999) by Alan W Black <awb@cstr.ed.ac.uk>

 * System functions

*/
#include <cstdio>
#include "siod.h"
#include "siodp.h"

static LISP sym_lambda = NIL;
static LISP sym_progn = NIL;

LISP setvar(LISP var,LISP val,LISP env)
{LISP tmp;
 if NSYMBOLP(var) err("wrong type of argument(non-symbol) to setvar",var);
 tmp = envlookup(var,env);
 if NULLP(tmp) return(VCELL(var) = val);
 return(CAR(tmp)=val);}
 
static LISP leval_setq(LISP args,LISP env)
{return(setvar(car(args),leval(car(cdr(args)),env),env));}

static LISP syntax_define(LISP args)
{
    if SYMBOLP(car(args)) 
                  return(args);
    else
    {
        need_n_cells(4);
        return(syntax_define(
                             cons(car(car(args)),
                             cons(cons(sym_lambda,
                             cons(cdr(car(args)),
                                  cdr(args))),
                                  NIL))));
    }
}
      
static LISP leval_define(LISP args,LISP env)
{LISP tmp,var,val;
 tmp = syntax_define(args);
 var = car(tmp);
 if NSYMBOLP(var) err("wrong type of argument(non-symbol) to define",var);
 val = leval(car(cdr(tmp)),env);
 tmp = envlookup(var,env);
 if NNULLP(tmp) return(CAR(tmp) = val);
 if NULLP(env) return(VCELL(var) = val);
 tmp = car(env);
 setcar(tmp,cons(var,car(tmp)));
 setcdr(tmp,cons(val,cdr(tmp)));
 return(val);}
 
static LISP leval_if(LISP *pform,LISP *penv)
{LISP args,env;
 args = cdr(*pform);
 env = *penv;
 if NNULLP(leval(car(args),env)) 
    *pform = car(cdr(args)); else *pform = car(cdr(cdr(args)));
 return(truth);}

static LISP arglchk(LISP x)
{
#if (!ENVLOOKUP_TRICK)
 LISP l;
 if SYMBOLP(x) return(x);
 for(l=x;CONSP(l);l=CDR(l));
 if NNULLP(l) err("improper formal argument list",x);
#endif
 return(x);}

static LISP leval_lambda(LISP args,LISP env)
{LISP body;
 if NULLP(cdr(cdr(args)))
   body = car(cdr(args));
  else body = cons(sym_progn,cdr(args));
 return(closure(env,cons(arglchk(car(args)),body)));}
                         
static LISP leval_progn(LISP *pform,LISP *penv)
{LISP env,l,next;
 env = *penv;
 gc_protect(&env);
 l = cdr(*pform);
 next = cdr(l);
 while (NNULLP(next)) 
 {
     leval(car(l),env);
     l=next;
     next=cdr(next);
 }
 gc_unprotect(&env);
 *pform = car(l); 
 return(truth);}

static LISP leval_or(LISP *pform,LISP *penv)
{LISP env,l,next,val;
 env = *penv;
 l = cdr(*pform);
 next = cdr(l);
 while(NNULLP(next))
   {val = leval(car(l),env);
    if NNULLP(val) {*pform = val; return(NIL);}
    l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

static LISP leval_and(LISP *pform,LISP *penv)
{LISP env,l,next;
 env = *penv;
 l = cdr(*pform);
 if NULLP(l) {*pform = truth; return(NIL);}
 next = cdr(l);
 while(NNULLP(next))
   {if NULLP(leval(car(l),env)) {*pform = NIL; return(NIL);}
    l=next;next=cdr(next);}
 *pform = car(l); 
 return(truth);}

static LISP leval_catch(LISP args,LISP env)
{struct catch_frame frame;
 int k;
 LISP l;
 volatile LISP val = NIL;
 frame.tag = leval(car(args),env);
 frame.next = catch_framep;
 k = setjmp(frame.cframe);
 catch_framep = &frame;
 if (k == 2)
   {catch_framep = frame.next;
    return(frame.retval);}
 for(l=cdr(args); NNULLP(l); l = cdr(l))
   val = leval(car(l),env);
 catch_framep = frame.next;
 return(val);}

static LISP lthrow(LISP tag,LISP value)
{struct catch_frame *l;
 for(l=catch_framep; l; l = (*l).next)
   if EQ((*l).tag,tag)
     {(*l).retval = value;
      longjmp((*l).cframe,2);}
 err("no *catch found with this tag",tag);
 return(NIL);}

static LISP leval_let(LISP *pform,LISP *penv)
{LISP env,l;
 l = cdr(*pform);
 env = *penv;
 *penv = extend_env(leval_args(car(cdr(l)),env),car(l),env);
 *pform = car(cdr(cdr(l)));
 return(truth);}

static LISP leval_quote(LISP args,LISP env)
{(void)env;
 return(car(args));}

static LISP leval_tenv(LISP args,LISP env)
{(void)args;
 return(env);}

static LISP leval_while(LISP args,LISP env)
{LISP l;
 while NNULLP(leval(car(args),env))
   for(l=cdr(args);NNULLP(l);l=cdr(l))
     leval(car(l),env);
 return(NIL);}

static LISP siod_typeof(LISP exp)
{
    switch TYPE(exp)
	{
	case tc_nil:
	    return NIL;
	case tc_cons:
	    return rintern("cons");
	case tc_flonum:
	    return rintern("flonum");
	case tc_string:
	    return rintern("string");
	case tc_subr_0:
	case tc_subr_1:
	case tc_subr_2:
	case tc_subr_3:
	case tc_subr_4:
	case tc_lsubr:
	case tc_fsubr:
	case tc_msubr:
	    return rintern("subr");
	case tc_c_file:
	    return rintern("c_file");
	case tc_closure:
	    return rintern("closure");
	default:
	    struct user_type_hooks *p;
	    EST_String tkb;
	    char ttkbuffer[1024];
	    p = get_user_type_hooks(TYPE(exp));
	    if (p->print_string)
	    {
		(*p->print_string)(exp, ttkbuffer);
		tkb = ttkbuffer;
		return rintern(tkb.after("#<").before(" "));
	    }
	    else
	    {
		if (p->name)
		    return rintern(p->name);
		else
		    return rintern("unknown");
	    }

	}
}

static LISP symbolp(LISP x)
{if SYMBOLP(x) return(truth); else return(NIL);}

LISP symbol_boundp(LISP x,LISP env)
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(truth);
 if EQ(VCELL(x),unbound_marker) return(NIL); else return(truth);}

LISP symbol_value(LISP x,LISP env)
{LISP tmp;
 if NSYMBOLP(x) err("not a symbol",x);
 tmp = envlookup(x,env);
 if NNULLP(tmp) return(CAR(tmp));
 tmp = VCELL(x);
 if EQ(tmp,unbound_marker) err("unbound variable",x);
 return(tmp);}

static LISP l_unwind_protect(LISP args, LISP env)
{
    // Do normal, if an error occurs do onerror
    jmp_buf * volatile local_errjmp = est_errjmp;
    est_errjmp = walloc(jmp_buf,1);
    volatile long local_errjmp_ok = errjmp_ok;
    errjmp_ok=1;   /* allow errjmps in here */
    volatile LISP r=NIL;
    volatile LISP previous_open_files = open_files;

    if (setjmp(*est_errjmp) != 0)
    {
	wfree(est_errjmp);
	est_errjmp = local_errjmp;
	errjmp_ok = local_errjmp_ok;
	siod_reset_prompt();
	// Close any that were opened below here
	close_open_files_upto(previous_open_files);
	if (siod_ctrl_c == TRUE)
	    err("forwarded through unwind-protect",NIL);
	r = leval(car(cdr(args)),env);
    }
    else
    {
	r = leval(car(args),env);
	wfree(est_errjmp);
	est_errjmp = local_errjmp;
	errjmp_ok = local_errjmp_ok;
    }

    return r;
}

static LISP oblistfn(void)
{return(copy_list(oblistvar));}

LISP let_macro(LISP form)
{LISP p,fl,al,tmp;
 fl = NIL;
 al = NIL;
 for(p=car(cdr(form));NNULLP(p);p=cdr(p))
  {tmp = car(p);
   if SYMBOLP(tmp) {fl = cons(tmp,fl); al = cons(NIL,al);}
   else {fl = cons(car(tmp),fl); al = cons(car(cdr(tmp)),al);}}
 p = cdr(cdr(form));
 if NULLP(cdr(p)) p = car(p); else p = cons(sym_progn,p);
 setcdr(form,cons(reverse(fl),cons(reverse(al),cons(p,NIL))));
 setcar(form,cintern("let-internal"));
 return(form);}
   
void init_subrs_core(void)
{
    gc_protect_sym(&sym_lambda,"lambda");
    gc_protect_sym(&sym_progn,"begin");

 init_fsubr("quote",leval_quote,
 "(quote DATA)\n\
  Return data (unevaluated).");
 init_fsubr("set!",leval_setq,
 "(set! SYMBOL VAL)\n\
  Set SYMBOL to have value VAL, returns VAL.");
 init_fsubr("define",leval_define,
 "(define (FUNCNAME ARG1 ARG2 ...) . BODY)\n\
 Define a new function call FUNCNAME with arguments ARG1, ARG2 ... and\n\
 BODY.");
 init_fsubr("lambda",leval_lambda,
 "(lambda (ARG1 ARG2 ...) . BODY)\n\
  Create closure (anonymous function) with arguments ARG1, ARG2 ... and \n\
 BODY.");
 init_msubr("if",leval_if,
 "(if COND TRUEPART FALSEPART)\n\
  If COND evaluates to non-nil evaluate TRUEPART and return result,\n\
  otherwise evaluate and return FALSEPART.  If COND is nil and FALSEPART\n\
  is nil, nil is returned.");
 init_fsubr("while",leval_while,
 "(while COND . BODY)\n\
  While COND evaluates to non-nil evaluate BODY.");
 init_msubr("begin",leval_progn,
 "(begin . BODY)\n\
  Evaluate s-expressions in BODY returning value of from last expression.");
 init_fsubr("*catch",leval_catch,
 "(*catch TAG . BODY)\n\
  Evaluate BODY, if a *throw occurs with TAG then return value specified\n\
  by *throw.");
 init_subr_2("*throw",lthrow,
 "(*throw TAG VALUE)\n\
  Jump to *catch with TAG, causing *catch to return VALUE.");
 init_msubr("let-internal",leval_let,
 "(let-internal STUFF)\n\
  Internal function used to implement let.");
 init_msubr("or",leval_or,
 "(or DISJ1 DISJ2 ...)\n\
  Evaluate each disjunction DISJn in turn until one evaluates to non-nil.\n\
  Otherwise return nil.");
 init_msubr("and",leval_and,
 "(and CONJ1 CONJ2 ... CONJN)\n\
  Evaluate each conjunction CONJn in turn until one evaluates to nil.\n\
  Otherwise return value of CONJN.");
 init_subr_1("typeof",siod_typeof,
 "(typeof OBJ)\n\
  Returns typeof of given object.");
 init_subr_1("symbol?",symbolp,
 "(symbol? DATA)\n\
  Returns t if DATA is a symbol, nil otherwise.");
 init_subr_2("symbol-bound?",symbol_boundp,
 "(symbol-bound? VARNAME)\n\
  Return t is VARNAME has a value, nil otherwise.");
 init_subr_2("symbol-value",symbol_value,
 "(symbol-value SYMBOLNAME)\n\
  Returns the value of SYMBOLNAME, an error is given SYMBOLNAME is not a\n\
  bound symbol.");
 init_fsubr("the-environment",leval_tenv,
 "(the-environment)\n\
  Returns the current (SIOD) environment.");
 init_fsubr("unwind-protect",l_unwind_protect,
 "(unwind-protect NORMALFORM ERRORFORM)\n\
 If an error is found while evaluating NORMALFORM catch it and evaluate\n\
 ERRORFORM and continue.  If an error occurs while evaluating NORMALFORM\n\
 all file open evaluating NORMALFORM up to the error while be automatically\n\
 closed.  Note interrupts (ctrl-c) is not caught by this function.");
 init_subr_0("oblist",oblistfn,
 "(oblist)\n\
  Return oblist.");
 init_subr_1("let-internal-macro",let_macro,
 "(let ((VAR1 VAL1) (VAR2 VAL2) ...) . BODY)\n\
  Evaluate BODY in an environment where VAR1 is set to VAL1, VAR2 is set\n\
  to VAL2 etc.");
 init_subr_3("set-symbol-value!",setvar,
 "(set-symbol-value! SYMBOLNAME VALUE)\n\
  Set SYMBOLNAME's value to VALUE, this is much faster than set! but use\n\
  with caution.");

}
