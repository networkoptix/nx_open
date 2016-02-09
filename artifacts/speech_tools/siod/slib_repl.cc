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

/****************************************************************/
/*                                                              */
/* read-eval print loop functions separated from main functions */
/* so LISP functions may be used without requiring full         */
/* evaluation to be linked (and termcap)                        */
#include <cstdio>
#include "EST_unix.h"
#include <cstdlib>
#include <cstring>
#include "EST_String.h"
#include "EST_cutils.h"
#include "siod.h"
#include "siodp.h"
#include "siodeditline.h"

int siod_repl(int interactive)
{
    int retval;
    LISP histsize;

    repl_prompt = siod_primary_prompt;
    
    /* Set history size (ignored if no command-line editing included) */
    histsize = siod_get_lval("editline_histsize",NULL);
    if (histsize != NIL)
	editline_histsize = get_c_int(histsize);
    editline_history_file = walloc(char,strlen(siod_prog_name)+10);
    sprintf(editline_history_file,".%s_history",siod_prog_name);
    if (siod_get_lval("editline_no_echo",NULL) != NULL)
	el_no_echo = 1;

    siod_interactive = interactive;
    siod_el_init();
    siod_fancy_getc = siod_el_getc;
    siod_fancy_ungetc = siod_el_ungetc;
    retval = repl_driver(1,0,NULL);
    if (interactive)
	cout << endl;

    return retval;
}

