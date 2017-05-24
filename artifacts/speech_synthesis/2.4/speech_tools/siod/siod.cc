/* Scheme In One Defun, but in C this time.
 
 *                    COPYRIGHT (c) 1988-1994 BY                            *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

*/

/*

gjc@paradigm.com or gjc@mitech.com or gjc@world.std.com

Paradigm Associates Inc          Phone: 617-492-6079
29 Putnam Ave, Suite 6
Cambridge, MA 02138

  */

/***************************************************************/
/* This has been modified to act as an interface to siod as an */
/* embedded Lisp module.                                       */
/* Also a (large) number of other functions have been added    */
/*                                                             */
/*    Alan W Black (awb@cstr.ed.ac.uk) 8th April 1996          */
/***************************************************************/
#include <cstdio>
#include "EST_unix.h"
#include <cstdlib>
#include <cstring>
#include "EST_String.h"
#include "EST_THash.h"
#include "EST_StringTrie.h"
#include "EST_cutils.h"
#include "EST_strcasecmp.h"
#include "siod.h"
#include "siodp.h"
#include "siodeditline.h"

#ifdef EST_SIOD_ENABLE_PYTHON
#include "slib_python.h"
#endif

extern "C" const char * repl_prompt;

template <> EST_String EST_THash<EST_String, EST_Regex *>::Dummy_Key = "DUMMY";
template <> EST_Regex *EST_THash<EST_String, EST_Regex *>::Dummy_Value = NULL;

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_THash.cc"

  Instantiate_TStringHash_T(EST_Regex *, hash_string_regex)
#endif

static EST_TStringHash<EST_Regex *> regexes(100);

int siod_init(int heap_size)
{
    /* Initialize siod */
    int actual_heap_size;

    if (heap_size == -1)  // unspecified by user
    {
	char *char_heap_size=getenv("SIODHEAPSIZE");
	if ((char_heap_size == 0) ||
	    (atoi(char_heap_size) < 1000))
	    actual_heap_size=ACTUAL_DEFAULT_HEAP_SIZE;
	else
	    actual_heap_size=atoi(char_heap_size);
    }
    else
	actual_heap_size = heap_size;

    init_storage(actual_heap_size);
    init_subrs();

    #ifdef EST_SIOD_ENABLE_PYTHON
    init_subrs_python();
    #endif

    return 0;
}

void siod_tidy_up()
{
    #ifdef EST_SIOD_ENABLE_PYTHON
    python_tidy_up();
    #endif

    close_open_files();
}

LISP siod_get_lval(const char *name,const char *message)
{
    // returns value of variable name.  If not set gives an error 
    LISP iii, rval=NIL;

    iii = rintern(name);

    // value or NIL if unset
    if (symbol_boundp(iii,current_env) == NIL)
    {
	if (message != NULL)
	    err(message,iii);
    }
    else
	rval = symbol_value(iii, current_env);

    return rval;
}

LISP siod_set_lval(const char *name,LISP val)
{
    // set variable name to val
    LISP iii, rval;
    
    iii = rintern(name);

    rval = setvar(iii,val,current_env);

    return rval;
}

LISP siod_assoc_str(const char *key,LISP alist)
{
    // assoc without going through LISP atoms 
    // made get_c_string inline for optimization
    LISP l,lc,lcc;

    for (l=alist; CONSP(l); l=CDR(l))
    {
	lc = CAR(l);
	if (CONSP(lc))
	{
	    lcc = CAR(lc);
	    if (NULLP(lcc)) continue;
	    else if TYPEP(lcc,tc_symbol)
	    {
		if (strcmp(key,PNAME(lcc))==0)
		    return lc;
	    }
	    else if TYPEP(lcc,tc_flonum)
	    {
		if (FLONMPNAME(lcc) == NULL)
		{
		    char b[TKBUFFERN];
		    sprintf(b,"%g",FLONM(lcc));
		    FLONMPNAME(lcc) = (char *)must_malloc(strlen(b)+1);
		    sprintf(FLONMPNAME(lcc),"%s",b);
		}
		if (strcmp(key,FLONMPNAME(lcc))==0)
		    return lc;
	    }
	    else if TYPEP(lcc,tc_string)
	    {
		if (strcmp(key,lcc->storage_as.string.data)==0)
		    return lc;
	    }
	    else
		continue;
	}
    }
    return NIL;
}

LISP siod_member_str(const char *key,LISP list)
{
    // member without going through LISP atoms 
    LISP l;

    for (l=list; CONSP(l); l=CDR(l))
	if (strcmp(key,get_c_string(CAR(l))) == 0)
	    return l;

    return NIL;
}

LISP siod_regex_member_str(const EST_String &key,LISP list)
{
    // Check the regexs in LIST against key
    LISP l;

    for (l=list; CONSP(l); l=CDR(l))
	if (key.matches(make_regex(get_c_string(CAR(l)))))
	    return l;

    return NIL;
}

LISP siod_member_int(const int key,LISP list)
{
    // member without going through LISP atoms 
    LISP l;

    for (l=list; CONSP(l); l=CDR(l))
	if (key == get_c_int(CAR(l)))
	    return l;
    return NIL;
}
	 
int siod_llength(LISP list)
{
    // length of string;
    int len;
    LISP l;

    for (len=0,l=list; CONSP(l); l=CDR(l),len++);
	
    return len;

}

LISP siod_nth(int n,LISP list)
{
    // nth member -- first member is 0;
    int i;
    LISP l;

    for (i=0,l=list; CONSP(l); l=CDR(l),i++)
	if (i == n)
	    return car(l);
	
    return NIL;

}

int siod_atomic_list(LISP list)
{
    // TRUE is list only contains atoms
    LISP p;

    for (p=list; p != NIL; p=cdr(p))
	if (CONSP(car(p)))
	    return FALSE;

    return TRUE;
}

int siod_eof(LISP item)
{
    // TRUE if item is what siod denotes as eof
    if (CONSP(item) &&
	(cdr(item) == NIL) &&
	(SYMBOLP(car(item))) &&
	(strcmp("eof",get_c_string(car(item))) == 0))
	return TRUE;
    else
	return FALSE;
}

LISP quote(LISP l)
{
    // Add quote round a Lisp expression
    return cons(rintern("quote"),cons(l,NIL));
}

LISP siod_last(LISP list)
{
    LISP l;

    if ((list == NIL) || (NCONSP(list)))
	return NIL;
    else
    {
	for (l=list; cdr(l) != NIL; l=cdr(l));
	return l;
    }
}

int get_param_int(const char *name, LISP params, int defval)
{
    // Look up name in params and return value if present or 
    // defval if not present 
    LISP pair;

    pair = siod_assoc_str(name,params);

    if (pair == NIL)
	return defval;
    else  if FLONUMP(car(cdr(pair)))
	return (int)FLONM(car(cdr(pair)));
    else
    {
	cerr << "param " << name << " not of type int" << endl;
	err("",NIL);
	return -1;
    }

}

float get_param_float(const char *name, LISP params, float defval)
{
    // Look up name in params and return value if present or 
    // defval if not present 
    LISP pair;

    pair = siod_assoc_str(name,params);

    if (pair == NIL)
	return defval;
    else  if (FLONUMP(car(cdr(pair))))
	return (float)FLONM(car(cdr(pair)));
    else
    {
	cerr << "param " << name << " not of type float" << endl;
	err("",NIL);
	return -1;
    }

}

const char *get_param_str(const char *name, LISP params, const char *defval)
{
    // Look up name in params and return value if present or 
    // defval if not present
    LISP pair;

    pair = siod_assoc_str(name,params);

    if (pair == NIL)
	return defval;
    else
	return get_c_string(car(cdr(pair)));
}

LISP get_param_lisp(const char *name, LISP params, LISP defval)
{
    // Look up name in params and return value if present or 
    // defval if not present 
    LISP pair;

    pair = siod_assoc_str(name,params);

    if (pair == NIL)
	return defval;
    else
	return car(cdr(pair));
}

LISP make_param_str(const char *name,const char *val)
{
    return cons(rintern(name),cons(rintern(val),NIL));
}

LISP make_param_int(const char *name, int val)
{
    return cons(rintern(name),cons(flocons(val),NIL));
}

LISP make_param_float(const char *name, float val)
{
    return cons(rintern(name),cons(flocons(val),NIL));
}

LISP make_param_lisp(const char *name,LISP val)
{
    return cons(rintern(name),cons(val,NIL));
}

EST_Regex &make_regex(const char *r)
{
    // Return pointer to existing regex if its already been created
    // otherwise create a new one for this r.
    EST_Regex *rx;
    EST_String sr = r;
    int found;

    rx = regexes.val(sr,found);
    if (!found)
    {
	rx = new EST_Regex(r);
	regexes.add_item(sr,rx);
    }

    return *rx;
}

LISP apply_hooks(LISP hooks,LISP arg)
{
    //  Apply each function in hooks to arg returning value from 
    // final application (or arg itself)
    LISP h,r;

    r = arg;
    
    if (hooks && (!CONSP(hooks)))  // singleton
	r = leval(cons(hooks,cons(quote(arg),NIL)),NIL);
    else
	for (h=hooks; h != NIL; h=cdr(h))
	    r = leval(cons(car(h),cons(quote(arg),NIL)),NIL);
    return r;
}

LISP apply_hooks_right(LISP hooks,LISP args)
{
    // The above version neither quotes its arguments properly of deals
    // with lists of arguments so here's a better one
    //  Apply each function in hooks to arg returning value from 
    // final application (or arg itself)
    LISP h,r;

    if (hooks == NIL)
	r = args;
    else if (!CONSP(hooks))  // singleton
	r = apply(hooks,args);
    else
	for (r=args,h=hooks; h != NIL; h=cdr(h))
	    r = apply(car(h),r);
    return r;
}

LISP apply(LISP func,LISP args)
{
    LISP qa,a;

    for (qa=NIL,a=args; a; a=cdr(a))
	qa = cons(quote(car(a)),qa);
    return leval(cons(func,reverse(qa)),NIL);
}

LISP stringexplode(const char *str)
{
    // Explode character string into list of symbols one for each char
    LISP l=NIL;
    unsigned int i;
    char id[2];
    id[1] = '\0';

    for (i=0; i < strlen(str); i++)
    {
	id[0] = str[i];
	l = cons(rintern(id),l);
    }

    return reverse(l);
}

/* Editline completion functions */
    
char **siod_variable_generator(char *text,int length)
{
    LISP l,lmatches;
    const char *name;
    char **matches = NULL;
    int i;

    /* Return the next name which partially matches from the command list. */
    for(lmatches=NIL,l=oblistvar;CONSP(l);l=CDR(l))
    {
	if (VCELL(car(l)) == NIL) continue;
	switch(TYPE(VCELL(CAR(l))))
	{
	  case tc_subr_0:
	  case tc_subr_1:
	  case tc_subr_2:
	  case tc_subr_3:
	  case tc_subr_4:
	  case tc_lsubr:
	  case tc_fsubr:
	  case tc_msubr:
	  case tc_closure:
	    continue;
	  default:
            /* only return names of nonfunctions (sometimes too restrictive) */
	    name = PNAME(CAR(l));
	    if (strncmp(name, text, length) == 0)
		lmatches = cons(CAR(l),lmatches);
	}
    }

    /* Need to return the matches in a char** */
    matches = walloc(char *,siod_llength(lmatches)+1);
    for (l=lmatches,i=0; l; l=cdr(l),i++)
	matches[i] = wstrdup(PNAME(car(l)));
    matches[i] = NULL;

    return matches;
}

char **siod_command_generator (char *text,int length)
{
    LISP l,lmatches;
    const char *name;
    char **matches = NULL;
    int i;

    /* Return the next name which partially matches from the command list. */
    for(lmatches=NIL,l=oblistvar;CONSP(l);l=CDR(l))
    {
	if (VCELL(car(l)) == NIL) continue;
	switch(TYPE(VCELL(CAR(l))))
	{
	  case tc_subr_0:
	  case tc_subr_1:
	  case tc_subr_2:
	  case tc_subr_3:
	  case tc_subr_4:
	  case tc_lsubr:
	  case tc_fsubr:
	  case tc_msubr:
	  case tc_closure:
            /* only return names of functions */
	    name = PNAME(CAR(l));
	    if (strncmp(name, text, length) == 0)
		lmatches = cons(CAR(l),lmatches);
	  default: continue;
	}
    }

    /* Need to return the matches in a char** */
    matches = walloc(char *,siod_llength(lmatches)+1);
    for (l=lmatches,i=0; l; l=cdr(l),i++)
	matches[i] = wstrdup(PNAME(car(l)));
    matches[i] = NULL;

    return matches;
}

void siod_list_to_strlist(LISP l, EST_StrList &a)
{
    // copy l into a
    LISP b;

    a.clear();

    for (b=l; b != NIL; b=cdr(b))
	a.append(get_c_string(car(b)));

}

LISP siod_strlist_to_list(EST_StrList &a)
{
    // copy a into l
    LISP b=NIL;;
    EST_Litem *p;

    for (p=a.head(); p != 0; p=p->next())
	b = cons(rintern(a(p)),b);

    return reverse(b);
}

