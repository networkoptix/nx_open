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
/*  library and Scheme-in-one-defun in particular.  All modifications to    */
/*  to this work are continued with the same copyright above.  That is      */
/*  This version editline does not have the the "no commercial use"         */
/*  restriction that some of the rest of the EST library may have           */
/*  awb Dec 30 1998                                                         */
/*                                                                          */
/****************************************************************************/
/*  $Revision: 1.3 $
**
**  History and file completion functions for editline library.
*/
#include "editline.h"


#if	defined(NEED_STRDUP)
/*
**  Return an allocated copy of a string.
*/
char * strdup(char *p)
{
    char	*new;

    if ((new = NEW(char, strlen(p) + 1)) != NULL)
	(void)strcpy(new, p);
    return new;
}
#endif	/* defined(NEED_STRDUP) */

/*
**  strcmp-like sorting predicate for qsort.
*/
STATIC int compare(CONST void *p1,CONST void *p2)
{
    CONST char	**v1;
    CONST char	**v2;

    v1 = (CONST char **)p1;
    v2 = (CONST char **)p2;
    return strcmp(*v1, *v2);
}

/*
**  Fill in *avp with an array of names that match file, up to its length.
**  Ignore . and .. .
*/
STATIC int FindMatches(char *dir,char *file,char ***avp)
{
#if !defined(SYSTEM_IS_WIN32)
    char	**av;
    char	**neww;
    char	*p;
    DIR		*dp;
    DIRENTRY	*ep;
    ESIZE_T	ac;
    ESIZE_T	len;

    if ((dp = opendir(dir)) == NULL)
	return 0;

    av = NULL;
    ac = 0;
    len = strlen(file);
    while ((ep = readdir(dp)) != NULL) {
	p = ep->d_name;
	if (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0')))
	    continue;
	if (len && strncmp(p, file, len) != 0)
	    continue;

	if ((ac % MEM_INC) == 0) {
	    if ((neww = NEW(char*, ac + MEM_INC)) == NULL)
		break;
	    if (ac) {
		COPYFROMTO(neww, av, ac * sizeof (char **));
		DISPOSE(av);
	    }
	    *avp = av = neww;
	}

	if ((av[ac] = STRDUP(p)) == NULL) {
	    if (ac == 0)
		DISPOSE(av);
	    break;
	}
	ac++;
    }

    /* Clean up and return. */
    (void)closedir(dp);
    if (ac)
	qsort(av, ac, sizeof (char **), compare);
    return ac;
#else
    *avp=NULL;
    return 0;
#endif
}

/*
**  Split a pathname into allocated directory and trailing filename parts.
*/
STATIC int SplitPath(char *path,char **dirpart,char **filepart)
{
    static char	DOT[] = ".";
    char	*dpart;
    char	*fpart;

    if ((fpart = strrchr(path, '/')) == NULL) {
	if ((dpart = STRDUP(DOT)) == NULL)
	    return -1;
	if ((fpart = STRDUP(path)) == NULL) {
	    DISPOSE(dpart);
	    return -1;
	}
    }
    else {
	if ((dpart = STRDUP(path)) == NULL)
	    return -1;
	dpart[fpart - path] = '\0';
	if ((fpart = STRDUP(++fpart)) == NULL) {
	    DISPOSE(dpart);
	    return -1;
	}
	if (dpart[0] == '\0')  /* special case for root */
	{
	    dpart[0] = '/';
	    dpart[1] = '\0';
	}
    }
    *dirpart = dpart;
    *filepart = fpart;
    return 0;
}

/*
**  Attempt to complete the pathname, returning an allocated copy.
**  Fill in *unique if we completed it, or set it to 0 if ambiguous.
*/
char *rl_complete(char *pathname,int *unique)
{
    char	**av;
    char	*dir;
    char	*file;
    char	*neww;
    char	*p;
    ESIZE_T	ac;
    ESIZE_T	end;
    ESIZE_T	i;
    ESIZE_T	j;
    ESIZE_T	len;

    if (SplitPath(pathname, &dir, &file) < 0)
	return NULL;
    if ((ac = FindMatches(dir, file, &av)) == 0) {
	DISPOSE(dir);
	DISPOSE(file);
	return NULL;
    }

    p = NULL;
    len = strlen(file);
    if (ac == 1) {
	/* Exactly one match -- finish it off. */
	*unique = 1;
	j = strlen(av[0]) - len + 2;
	if ((p = NEW(char, j + 1)) != NULL) {
	    COPYFROMTO(p, av[0] + len, j);
	    if ((neww = NEW(char, strlen(dir) + strlen(av[0]) + 2)) != NULL) {
		(void)strcpy(neww, dir);
		(void)strcat(neww, "/");
		(void)strcat(neww, av[0]);
		rl_add_slash(neww, p);
		DISPOSE(neww);
	    }
	}
    }
    else {
	*unique = 0;
	if (len) {
	    /* Find largest matching substring. */
	    for (i = len, end = strlen(av[0]); i < end; i++)
		for (j = 1; j < ac; j++)
		    if (av[0][i] != av[j][i])
			goto breakout;
  breakout:
	    if (i > len) {
		j = i - len + 1;
		if ((p = NEW(char, j)) != NULL) {
		    COPYFROMTO(p, av[0] + len, j);
		    p[j - 1] = '\0';
		}
	    }
	}
    }

    /* Clean up and return. */
    DISPOSE(dir);
    DISPOSE(file);
    for (i = 0; i < ac; i++)
	DISPOSE(av[i]);
    DISPOSE(av);
    return p;
}

/*
**  Return all possible completions.
*/
int rl_list_possib(char *pathname,char ***avp)
{
    char	*dir;
    char	*file, *path, *tt;
    int		ac,i;

    if (SplitPath(pathname, &dir, &file) < 0)
	return 0;
    ac = FindMatches(dir, file, avp);
    /* Identify directories with trailing / */
    for (i = 0; i < ac; i++)  
    {
	path = walloc(char,strlen(dir)+strlen((*avp)[i])+3);
	sprintf(path,"%s/%s",dir,(*avp)[i]);
	if (el_is_directory(path))
	{
	    tt = walloc(char,strlen((*avp)[i])+2);
	    sprintf(tt,"%s/",(*avp)[i]);
	    wfree((*avp)[i]);
	    (*avp)[i] = tt;
	}
	wfree(path);
    }
    DISPOSE(dir);
    DISPOSE(file);
    return ac;
}
