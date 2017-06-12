/*  
 *                   COPYRIGHT (c) 1988-1994 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

 * Reorganization of files (Mar 1999) by Alan W Black <awb@cstr.ed.ac.uk>

 * Documentation support

*/
#include <cstdio>
#include "EST_cutils.h"
#include "siod.h"
#include "siodp.h"
#include "siodeditline.h"

void setdoc(LISP name,LISP doc)
{
    /* Set documentation string for name */
    LISP lpair = assq(name,siod_docstrings);
    if (lpair == NIL)
	siod_docstrings = cons(cons(name,doc),siod_docstrings);
    else
    {
	cerr << "SIOD: duplicate builtin function: " <<
	    get_c_string(name) << endl;
	cerr << "SIOD: probably an error" << endl;
	CDR(lpair) = doc;
    }
}

static LISP siod_doc(LISP args,LISP penv)
{
    /* Return documentation string for sym */
    (void)penv;
    LISP lpair,val,tmp,code;
    LISP var_docstrings;

    if (TYPE(car(args)) != tc_symbol)
	return rintern("No documentation available for non-symbol.");
    tmp = envlookup(car(args),penv);
    if NNULLP(tmp) 
	val = car(tmp);
    else
	val = VCELL(car(args));
    if EQ(val,unbound_marker)
	return rintern("Symbol is unbound.");
    else
    {
	var_docstrings = symbol_value(rintern("var-docstrings"),penv);
	lpair = assq(car(args),var_docstrings);
	if (lpair)
	    return cdr(lpair);
	else
	    rintern("No documentation available for symbol.");	    
    }
    switch (TYPE(val))
    {
      case tc_subr_0:
      case tc_subr_1:
      case tc_subr_2:
      case tc_subr_3:
      case tc_subr_4:
      case tc_lsubr:
      case tc_fsubr:
      case tc_msubr:
	lpair = assq(car(args),siod_docstrings);
	if (lpair != NIL)
	    return cdr(lpair);
	else
	    return rintern("No documentation available for builtin function.");
	break;
      case tc_closure:
	code = val->storage_as.closure.code;
	if ((TYPE(cdr(code)) == tc_cons) &&
	    (TYPE(car(cdr(cdr(code)))) == tc_string))
	    return car(cdr(cdr(code)));
	else
	    return rintern("No documentation available for user-defined function.");
      default:
	return rintern("No documentation available for symbol.");
    }
	
    return rintern("No documentation available for symbol.");
}

static LISP siod_all_function_docstrings(void)
{
    // Returns all an assoc list of ALL functions that have any form 
    // of documentation strings, internal functions or user defined.
    LISP docs = siod_docstrings;
    
    // But we need user defined function with docstrings too.
    // The docustring must start with a ( to be included
    LISP l = oblistvar;
    LISP code,val;

    // Search the oblist for functions
    for(;CONSP(l);l=CDR(l))
    {
	if (VCELL(car(l)) == NIL) continue;
	switch(TYPE(VCELL(CAR(l))))
	{
	  case tc_closure:
	    val = VCELL(CAR(l));
	    code = val->storage_as.closure.code;
	    if ((CONSP(code)) &&
		(CONSP(cdr(code))) &&
		(CONSP(cdr(cdr(code)))) &&
		(TYPE(car(cdr(cdr(code)))) == tc_string))
		docs = cons(cons(car(l),car(cdr(cdr(code)))),docs);
	  default:
	    continue;
	}
    }

    return docs;
}

static int sort_compare_docstrings(const void *x, const void *y)
{
    LISP a=*(LISP *)x;
    LISP b=*(LISP *)y;

    return EST_strcasecmp(get_c_string(car(a)),get_c_string(car(b)));
}

static void siod_print_docstring(const char *symname, 
				 const char *docstring, FILE *fp)
{
    // Print to fp a texinfo list item for this description
    // Take the first line of the docstring as the label, and also remove
    // any indentation in the remainder of the lines
    int i,state;
    (void)symname;
    EST_String ds = docstring;
    const char *dsc;

    if (ds.contains(make_regex("\\[see .*\\]$")))
    {   // Contains a cross reference so replace it with texi xref command
	EST_String rest, ref;
	rest = ds.before(make_regex("\\[see [^\n]*\\]$"));
	ref = ds.after(rest);
	ref = ref.after("[see ");
	ref = ref.before("]");
	ds = rest + EST_String("[\\@pxref\\{") + ref + EST_String("\\}]");
    }

    dsc = ds;

    fprintf(fp,"@item ");
    for (state=0,i=0; dsc[i] != '\0'; i++)
    {
	if (((dsc[i] == '@') ||
	     (dsc[i] == '{') ||
	     (dsc[i] == '}')) &&
	    ((i == 0) ||
	     (dsc[i-1] != '\\')))
	    putc('@',fp);
	if ((dsc[i] == '\\') &&
	    ((dsc[i+1] == '@') ||
	     (dsc[i+1] == '{') ||
	     (dsc[i+1] == '}')))
	    continue;
	else if (state == 0)
	{
	    putc(dsc[i],fp);
	    if (dsc[i] == '\n')
		state = 1;
	}
	else if (state == 1)
	    if (dsc[i] != ' ')
	    {
		putc(dsc[i],fp);
		state = 0;
	    }
    }
    fprintf(fp,"\n");
}

static LISP siod_sort_and_dump_docstrings(LISP type,LISP filefp)
{
    // sort docstrings then dump them to filefp as a texinfo list
    LISP *array,l,docstrings;
    int num_strings;
    int i;

    if (streq(get_c_string(type),"function"))
	docstrings = siod_all_function_docstrings();
    else if (streq(get_c_string(type),"features"))
	docstrings = symbol_value(rintern("ff_docstrings"),NIL);
    else
	docstrings = symbol_value(rintern("var-docstrings"),NIL);
	
    num_strings = siod_llength(docstrings);
    array = walloc(LISP,num_strings);
    for (l=docstrings,i=0; i < num_strings; i++,l=cdr(l))
	array[i] = car(l);
    qsort(array,num_strings,sizeof(LISP),sort_compare_docstrings);

    for (i=0; i < num_strings; i++)
	siod_print_docstring(get_c_string(car(array[i])),
			     get_c_string(cdr(array[i])),
			     get_c_file(filefp,stdout));

    wfree(array);

    return NIL;

}

const char *siod_docstring(const char *symbol)
{
    LISP doc;

    doc = siod_doc(cons(rintern(symbol),NIL),NIL);

    return get_c_string(doc);
}

const char *siod_manual_sym(const char *symbol)
{
    // For siodline 
    LISP info;

    info = leval(cons(rintern("manual-sym"),
		      cons(quote(rintern(symbol)),NIL)),NIL);

    return get_c_string(info);
}

void siod_saydocstring(const char *symbol)
{
    // This isn't guaranteed to work but might be ok sometimes

    leval(cons(rintern("tts_text"),
	       cons(cons(rintern("doc"),cons(rintern(symbol),NIL)),
		    cons(NIL,NIL))),NIL);

}

void init_subrs_doc(void)
{
 init_fsubr("doc",siod_doc,
 "(doc SYMBOL)\n\
  Return documentation for SYMBOL.");
 init_subr_2("sort-and-dump-docstrings",siod_sort_and_dump_docstrings,
 "(sort-and-dump-docstrings DOCSTRINGS FILEFP)\n\
  DOCSTRINGS is an assoc list of name and document string var-docstrings\n\
  or func-docstrings.  This very individual function sorts the list and \n\
  prints out the documentation strings as texinfo list members to FILEFP.");

}
