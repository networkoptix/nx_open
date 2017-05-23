/*  
 *                   COPYRIGHT (c) 1988-1994 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

 * Reorganization of files (Mar 1999) by Alan W Black <awb@cstr.ed.ac.uk>

 * math functions

*/
#include <cstdio>
#include "siod.h"
#include "siodp.h"

LISP numberp(LISP x)
{if FLONUMP(x) return(truth); else return(NIL);}

static LISP lplus(LISP args)
{
    LISP l;
    double sum;
    for (sum=0.0,l=args; l != NIL; l=cdr(l))
    {
	if (NFLONUMP(car(l))) err("wrong type of argument to plus",car(l));
	sum += FLONM(car(l));
    }
    return flocons(sum);
}

static LISP ltimes(LISP args)
{
    LISP l;
    double product;
    for (product=1.0,l=args; l != NIL; l=cdr(l))
    {
	if (NFLONUMP(car(l))) err("wrong type of argument to times",car(l));
	product *= FLONM(car(l));
    }
    return flocons(product);
}

static LISP difference(LISP x,LISP y)
{if NFLONUMP(x) err("wrong type of argument(1st) to difference",x);
 if NFLONUMP(y) err("wrong type of argument(2nd) to difference",y);
 return(flocons(FLONM(x) - FLONM(y)));}

static LISP quotient(LISP x,LISP y)
{if NFLONUMP(x) err("wrong type of argument(1st) to quotient",x);
 if NFLONUMP(y) err("wrong type of argument(2nd) to quotient",y);
 return(flocons(FLONM(x)/FLONM(y)));}

static LISP greaterp(LISP x,LISP y)
{if NFLONUMP(x) err("wrong type of argument(1st) to greaterp",x);
 if NFLONUMP(y) err("wrong type of argument(2nd) to greaterp",y);
 if (FLONM(x)>FLONM(y)) return(truth);
 return(NIL);}

static LISP lessp(LISP x,LISP y)
{if NFLONUMP(x) err("wrong type of argument(1st) to lessp",x);
 if NFLONUMP(y) err("wrong type of argument(2nd) to lessp",y);
 if (FLONM(x)<FLONM(y)) return(truth);
 return(NIL);}

static LISP l_nint(LISP number)
{
    if (TYPEP(number,tc_flonum))
    {
	int iii = (int)(FLONM(number)+0.5);
	return flocons(iii);
    }
    else if (TYPEP(number,tc_symbol))
    {
	int iii = (int)(atof(get_c_string(number))+0.5);
	return flocons(iii);
    }
    else
	err("nint: argument not a number",number);

    return NIL;
}

static LISP l_log(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(log(FLONM(n)));
    else
	err("log: not a number",n);

    return NIL;
}

static LISP l_rand()
{
    double r = (double)abs(rand())/(double)RAND_MAX;
    
    return flocons(r);
}

static LISP l_srand(LISP seed)
{
    if (seed && (TYPEP(seed,tc_flonum)))
	srand((int) FLONM(seed));
    else
	err("srand: not a number", seed);
    return NIL;
}

static LISP l_exp(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(exp(FLONM(n)));
    else
	err("exp: not a number",n);
    return NIL;
}

static LISP l_sin(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(sin(FLONM(n)));
    else
	err("sin: not a number",n);
    return NIL;
}

static LISP l_cos(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(cos(FLONM(n)));
    else
	err("cos: not a number",n);
    return NIL;
}

static LISP l_tan(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(tan(FLONM(n)));
    else
	err("tan: not a number",n);
    return NIL;
}

static LISP l_asin(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(asin(FLONM(n)));
    else
	err("asin: not a number",n);
    return NIL;
}

static LISP l_acos(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(acos(FLONM(n)));
    else
	err("acos: not a number",n);
    return NIL;
}

static LISP l_atan(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(atan(FLONM(n)));
    else
	err("atan: not a number",n);
    return NIL;
}

static LISP l_sqrt(LISP n)
{
    if (n && (TYPEP(n,tc_flonum)))
	return flocons(sqrt(FLONM(n)));
    else
	err("sqrt: not a number",n);
    return NIL;
}

static LISP l_pow(LISP x, LISP y)
{
    if (x && (TYPEP(x,tc_flonum)) &&
	y && (TYPEP(y,tc_flonum)))
	return flocons(pow(FLONM(x),FLONM(y)));
    else
	err("pow: x or y not a number",cons(x,cons(y,NIL)));
    return NIL;
}

static LISP l_mod(LISP x, LISP y)
{
    if (x && (TYPEP(x,tc_flonum)) &&
	y && (TYPEP(y,tc_flonum)))
    {
	int a,b;

	a = (int)FLONM(x);
	b = (int)FLONM(y);
	if (b == 0)
	    err("mod: y cannot be 0",cons(x,cons(y,NIL)));

	return flocons((float)(a%b));
    }
    else
	err("mod: x or y not a number",cons(x,cons(y,NIL)));
    return NIL;
}

void init_subrs_math(void)
{
 init_subr_1("number?",numberp,
 "(number? DATA)\n\
  Returns t if DATA is a number, nil otherwise.");
 init_lsubr("+",lplus,
 "(+ NUM1 NUM2 ...)\n\
  Returns the sum of NUM1 and NUM2 ...  An error is given is any argument\n\
  is not a number.");
 init_subr_2("-",difference,
 "(- NUM1 NUM2)\n\
  Returns the difference between NUM1 and NUM2.  An error is given is any\n\
  argument is not a number.");
 init_lsubr("*",ltimes,
 "(* NUM1 NUM2 ...)\n\
  Returns the product of NUM1 and NUM2 ...  An error is given is any\n\
  argument is not a number.");
 init_subr_2("/",quotient,
 "(/ NUM1 NUM2)\n\
  Returns the quotient of NUM1 and NUM2.  An error is given is any\n\
  argument is not a number.");
 init_subr_2(">",greaterp,
 "(> NUM1 NUM2)\n\
  Returns t if NUM1 is greater than NUM2, nil otherwise.  An error is\n\
  given is either argument is not a number.");
 init_subr_2("<",lessp,
 "(< NUM1 NUM2)\n\
  Returns t if NUM1 is less than NUM2, nil otherwise.  An error is\n\
  given is either argument is not a number.");
 init_subr_1("nint",l_nint,
 "(nint NUMBER)\n\
  Returns nearest int to NUMBER.");
 init_subr_1("log",l_log,
 "(log NUM)\n\
 Return natural log of NUM.");
 init_subr_0("rand",l_rand,
 "(rand)\n\
 Returns a pseudo random number between 0 and 1 using the libc rand()\n\
 function.");
 init_subr_1("srand",l_srand,
 "(srand SEED)\n\
 Seeds the libc pseudo random number generator with the integer SEED.");
 init_subr_1("exp",l_exp,
 "(exp NUM)\n\
 Return e**NUM.");
 init_subr_1("sin",l_sin,
 "(sin NUM)\n\
 Return sine of NUM.");
 init_subr_1("cos",l_cos,
 "(cos NUM)\n\
 Return cosine of NUM.");
 init_subr_1("tan",l_tan,
 "(tan NUM)\n\
 Return tangent of NUM.");
 init_subr_1("asin",l_asin,
 "(asin NUM)\n\
 Return arcsine of NUM.");
 init_subr_1("acos",l_acos,
 "(acos NUM)\n\
 Return arccosine of NUM.");
 init_subr_1("atan",l_atan,
 "(atan NUM)\n\
 Return arctangent of NUM.");
 init_subr_1("sqrt",l_sqrt,
 "(sqrt NUM)\n\
 Return square root of NUM.");
 init_subr_2("pow",l_pow,
 "(pow X Y)\n\
 Return X**Y.");
 init_subr_2("%",l_mod,
 "(% X Y)\n\
 Return X%Y.");

}
