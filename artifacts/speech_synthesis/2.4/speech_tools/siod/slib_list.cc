/*  
 *                   COPYRIGHT (c) 1988-1994 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

 * Reorganization of files (Mar 1999) by Alan W Black <awb@cstr.ed.ac.uk>

 * General list functions

*/
#include <cstdio>
#include "siod.h"
#include "siodp.h"

static LISP llength(LISP obj)
{LISP l;
 long n;
 switch TYPE(obj)
   {case tc_string:
      return(flocons(obj->storage_as.string.dim));
    case tc_double_array:
      return(flocons(obj->storage_as.double_array.dim));
    case tc_long_array:
      return(flocons(obj->storage_as.long_array.dim));
    case tc_lisp_array:
      return(flocons(obj->storage_as.lisp_array.dim));
    case tc_nil:
      return(flocons(0.0));
    case tc_cons:
      for(l=obj,n=0;CONSP(l);l=CDR(l),++n) INTERRUPT_CHECK();
      if NNULLP(l) err("improper list to length",obj);
      return(flocons(n));
    default:
      return(err("wrong type of argument to length",obj));}}

LISP assoc(LISP x,LISP alist)
{LISP l,tmp;
 for(l=alist;CONSP(l);l=CDR(l))
   {tmp = CAR(l);
    if (CONSP(tmp) && equal(CAR(tmp),x)) return(tmp);
    INTERRUPT_CHECK();}
 if EQ(l,NIL) return(NIL);
 return(err("improper list to assoc",alist));}

LISP assq(LISP x,LISP alist)
{LISP l,tmp;
 for(l=alist;CONSP(l);l=CDR(l))
   {tmp = CAR(l);
    if (CONSP(tmp) && EQ(CAR(tmp),x)) return(tmp);
    INTERRUPT_CHECK();}
 if EQ(l,NIL) return(NIL);
 return(err("improper list to assq",alist));}

LISP setcar(LISP cell, LISP value)
{if NCONSP(cell) err("wrong type of argument to setcar",cell);
 return(CAR(cell) = value);}

LISP setcdr(LISP cell, LISP value)
{if NCONSP(cell) err("wrong type of argument to setcdr",cell);
 return(CDR(cell) = value);}

LISP delq(LISP elem,LISP l)
{if NULLP(l) return(l);
 STACK_CHECK(&elem);
 if EQ(elem,car(l)) return(cdr(l));
 setcdr(l,delq(elem,cdr(l)));
 return(l);}

LISP copy_list(LISP x)
{if NULLP(x) return(NIL);
 STACK_CHECK(&x);
 return(cons(car(x),copy_list(cdr(x))));}

static LISP eq(LISP x,LISP y)
{if EQ(x,y) return(truth); else return(NIL);}

LISP eql(LISP x,LISP y)
{if EQ(x,y) return(truth); else 
 if NFLONUMP(x) return(NIL); else
 if NFLONUMP(y) return(NIL); else
 if (FLONM(x) == FLONM(y)) return(truth);
 return(NIL);}

static LISP nullp(LISP x)
{if EQ(x,NIL) return(truth); else return(NIL);}

LISP siod_flatten(LISP tree)
{
    if (tree == NIL)
	return NIL;
    else if (consp(tree))
	return append(siod_flatten(car(tree)),siod_flatten(cdr(tree)));
    else
	return cons(tree,NIL);
}

LISP cons(LISP x,LISP y)
{LISP z;
 NEWCELL(z,tc_cons);
 CAR(z) = x;
 CDR(z) = y;
 return(z);}

LISP atomp(LISP x)
{
    if ((x==NIL) || CONSP(x)) 
	return NIL; 
    else 
	return truth;
}

LISP consp(LISP x)
{if CONSP(x) return(truth); else return(NIL);}

LISP car(LISP x)
{switch TYPE(x)
   {case tc_nil:
      return(NIL);
    case tc_cons:
      return(CAR(x));
    default:
      return(err("wrong type of argument to car",x));}}

LISP cdr(LISP x)
{switch TYPE(x)
   {case tc_nil:
      return(NIL);
    case tc_cons:
      return(CDR(x));
    default:
      return(err("wrong type of argument to cdr",x));}}

LISP equal(LISP a,LISP b)
{struct user_type_hooks *p;
 long atype;
 STACK_CHECK(&a);
 loop:
 INTERRUPT_CHECK();
 if EQ(a,b) return(truth);
 atype = TYPE(a);
 if (atype != TYPE(b)) return(NIL);
 switch(atype)
   {case tc_cons:
      if NULLP(equal(car(a),car(b))) return(NIL);
      a = cdr(a);
      b = cdr(b);
      goto loop;
    case tc_flonum:
      return((FLONM(a) == FLONM(b)) ? truth : NIL);
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
      return(NIL);
    default:
      p = get_user_type_hooks(atype);
      if (p->equal)
	return((*p->equal)(a,b));
      else if (p)  /* a user type */
	  return ((USERVAL(a) == USERVAL(b)) ? truth : NIL);
      else
	return(NIL);}}

LISP reverse(LISP l)
{LISP n,p;
 n = NIL;
 for(p=l;NNULLP(p);p=cdr(p)) n = cons(car(p),n);
 return(n);}

LISP append(LISP l1, LISP l2)
{LISP n=l2,p,rl1 = reverse(l1);
 for(p=rl1;NNULLP(p);p=cdr(p)) 
     n = cons(car(p),n);
 return(n);}

void init_subrs_list(void)
{
 init_subr_2("assoc",assoc,
  "(assoc KEY A-LIST)\n\
 Return pair with KEY in A-LIST or nil.");
 init_subr_1("length",llength,
  "(length LIST)\n\
  Return length of LIST, or 0 if LIST is not a list.");
 init_subr_1("flatten",siod_flatten,
  "(flatten LIST)\n\
  Return flatend list (list of all atoms in LIST).");
 init_subr_2("assq",assq,
 "(assq ITEM ALIST)\n\
  Returns pairs from ALIST whose car is ITEM or nil if ITEM is not in ALIST.");
 init_subr_2("delq",delq,
 "(delq ITEM LIST)\n\
  Destructively delete ITEM from LIST, returns LIST, if ITEM is not first\n\
  in LIST, cdr of LIST otherwise.  If ITEM is not in LIST, LIST is\n\
  returned unchanged." );
 init_subr_1("copy-list",copy_list,
 "(copy-list LIST)\n\
  Return new list with same members as LIST.");
  init_subr_2("cons",cons,
 "(cons DATA1 DATA2)\n\
  Construct cons pair whose car is DATA1 and cdr is DATA2.");
 init_subr_1("pair?",consp,
 "(pair? DATA)\n\
  Returns t if DATA is a cons cell, nil otherwise.");
 init_subr_1("car",car,
 "(car DATA1)\n\
  Returns car of DATA1.  If DATA1 is nil or a symbol, return nil.");
 init_subr_1("cdr",cdr,
 "(cdr DATA1)\n\
  Returns cdr of DATA1.  If DATA1 is nil or a symbol, return nil.");
 init_subr_2("set-car!",setcar,
 "(set-car! CONS1 DATA1)\n\
  Set car of CONS1 to be DATA1.  Returns CONS1. If CONS1 not of type\n\
  consp an error is is given.  This is a destructive operation.");
 init_subr_2("set-cdr!",setcdr,
 "(set-cdr! CONS1 DATA1)\n\
  Set cdr of CONS1 to be DATA1.  Returns CONS1. If CONS1 not of type\n\
  consp an error is is given.  This is a destructive operation.");
 init_subr_2("eq?",eq,
 "(eq? DATA1 DATA2)\n\
  Returns t if DATA1 and DATA2 are the same object.");
 init_subr_2("eqv?",eql,
 "(eqv? DATA1 DATA2)\n\
  Returns t if DATA1 and DATA2 are the same object or equal numbers.");
 init_subr_2("equal?",equal,
  "(equal? A B)\n\
  t if s-expressions A and B are recursively equal, nil otherwise.");
 init_subr_1("not",nullp,
 "(not DATA)\n\
  Returns t if DATA is nil, nil otherwise.");
 init_subr_1("null?",nullp,
 "(null? DATA)\n\
  Returns t if DATA is nil, nil otherwise.");
 init_subr_1("reverse",reverse,
 "(reverse LIST)\n\
  Returns destructively reversed LIST.");
 init_subr_2("append",append,
 "(append LIST1 LIST2)\n\
  Returns LIST2 appended to LIST1, LIST1 is distroyed.");
}
