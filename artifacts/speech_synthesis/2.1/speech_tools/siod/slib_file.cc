/*  
 *                   COPYRIGHT (c) 1988-1994 BY                             *
 *        PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.       *
 *        See the source file SLIB.C for more information.                  *

 * Reorganization of files (Mar 1999) by Alan W Black <awb@cstr.ed.ac.uk>

 * File functions

*/
#include <cstdio>
#include "siod.h"
#include "siodp.h"
#include "EST_Pathname.h"

static void siod_string_print(LISP exp, EST_String &sd);

LISP open_files = NIL;

void pprintf(FILE *fd,LISP exp,int indent,int width, int depth,int length)
{
    // A pretty printer for expressions
    // indent is the number of spaces to indent by
    // width is the maximum column we're allow to print to
    // depth is the we should print before ignoring it
    // length is the number of items in a list we should print
    int i,ll;
    LISP l;

    if (exp == NIL)
	fprintf(fd,"nil");
    else if (!consp(exp))
	fprintf(fd,"%s",(const char *)siod_sprint(exp));
    else 
    {
	EST_String p = siod_sprint(exp);
	if (p.length() < width-indent)
	    fprintf(fd,"%s",(const char *)p);
	else
	{
	    fprintf(fd,"(");
	    indent += 1;
	    if (depth == 0)
		fprintf(fd,"...");
	    else
	    {
		pprintf(fd,car(exp),indent,width,depth-1,length);
		for (ll=length,l=cdr(exp); l != NIL; l=cdr(l),ll--)
		{
		    fprintf(fd,"\n");
		    for (i=0; i<indent; i++)
			fprintf(fd," ");
		    if (ll == 0)
		    {
			pprintf(fd,rintern("..."),indent,width,
				depth-1,length);
			break;
		    }
		    else if (!consp(l))  // a dotted pair
		    {
			fprintf(fd," . %s",(const char *)siod_sprint(l));
			break;
		    }
		    else
			pprintf(fd,car(l),indent,width,depth-1,length);
		}
	    }
	    fprintf(fd,")");
	}
    }    
}

void pprint_to_fd(FILE *fd,LISP exp)
{
    pprintf(fd,exp,0,72,-1,-1);
    fprintf(fd,"\n");
}

static LISP siod_pprintf(LISP exp, LISP file)
{
    //  Pretty printer

    if ((file == NIL) ||
	(equal(file,rintern("t"))))
	pprint(exp);
    else
    {
	pprintf(get_c_file(file,stdout),exp,0,72,-1,-1);	
	fprintf(get_c_file(file,stdout),"\n");
    }
    return NIL;
}

void pprint(LISP exp)
{
    // Pretty print this expression to stdout

    pprint_to_fd(stdout,exp);
}

static LISP fflush_l(LISP p)
{
    if (p == NIL)
	fflush(stdout);
    else if NTYPEP(p,tc_c_file) 
	err("not a file",p);
    else
	fflush(p->storage_as.c_file.f);
    return NIL;
}

static void siod_string_print(LISP exp, EST_String &sd)
{
    LISP tmp;
    int i;

    switch TYPE(exp)
    {
      case tc_nil:
	sd += "nil";
	break;
      case tc_cons:
	sd += "(";
	siod_string_print(car(exp),sd);
	for(tmp=cdr(exp);CONSP(tmp);tmp=cdr(tmp))
	{
	    sd += " ";
	    siod_string_print(car(tmp),sd);
	}
	if NNULLP(tmp) 
	{
	    sd += " . ";
	    siod_string_print(tmp,sd);
	}
	sd += ")";
	break;
      case tc_flonum:
	if (FLONMPNAME(exp) == NULL)
	{
	    sprintf(tkbuffer,"%.8g",FLONM(exp));
	    FLONMPNAME(exp) = (char *)must_malloc(strlen(tkbuffer)+1);
	    sprintf(FLONMPNAME(exp),"%s",tkbuffer);
	}
	sprintf(tkbuffer,"%s",FLONMPNAME(exp));
	sd += tkbuffer;
	break;
      case tc_string:
	sd += "\"";
	for (i=0; exp->storage_as.string.data[i] != '\0'; i++)
	{
	    if (exp->storage_as.string.data[i] == '"')
		sd += "\\";
	    if (exp->storage_as.string.data[i] == '\\')
		sd += "\\";
	    sprintf(tkbuffer,"%c",exp->storage_as.string.data[i]);
	    sd += tkbuffer;
	}
	sd += "\"";
	break;
      case tc_symbol:
	sd += PNAME(exp);
	break;
      case tc_subr_0:
      case tc_subr_1:
      case tc_subr_2:
      case tc_subr_3:
      case tc_subr_4:
      case tc_lsubr:
      case tc_fsubr:
      case tc_msubr:
	sprintf(tkbuffer,"#<SUBR(%d) ",TYPE(exp));
	sd += tkbuffer;
	sd += (*exp).storage_as.subr.name;
	sd += ">";
	break;
      case tc_c_file:
	sprintf(tkbuffer,"#<FILE %p ",(void *)exp->storage_as.c_file.f);
	sd += tkbuffer;
	if (exp->storage_as.c_file.name)
	    sd += exp->storage_as.c_file.name;
	sd += ">";
        break;
      case tc_closure:
	sd += "#<CLOSURE ";
	siod_string_print(car((*exp).storage_as.closure.code),sd);
	sd += " ";
	siod_string_print(cdr((*exp).storage_as.closure.code),sd);
	sd += ">";
	break;
      default:
	struct user_type_hooks *p;
	p = get_user_type_hooks(TYPE(exp));
	if (p->print_string)
	  (*p->print_string)(exp, tkbuffer);
	else
	{
	    if (p->name)
		sprintf(tkbuffer,"#<%s %p>",p->name,(void *)exp);
	    else
		sprintf(tkbuffer,"#<UNKNOWN %d %p>",TYPE(exp),(void *)exp);
	}
	sd += tkbuffer;
    }
    return;
}

EST_String siod_sprint(LISP exp)
{
    EST_String r;

    r = "";
    siod_string_print(exp,r);

    return r;
}


static LISP fd_to_scheme_file(int fd, 
			      const char *name, 
			      const char *how,
			      int close_on_error)
{
  LISP sym;
  long flag;
  flag = no_interrupt(1);
  sym = newcell(tc_c_file);
  sym->storage_as.c_file.f = (FILE *)NULL;
  sym->storage_as.c_file.name = (char *)NULL;

  if (fd != fileno(stderr))
      open_files = cons(sym,open_files);
  sym->storage_as.c_file.name = (char *) must_malloc(strlen(name)+1);
  if (fd == fileno(stdin))
      sym->storage_as.c_file.f = stdin;
  else   if (fd == fileno(stdout))
      sym->storage_as.c_file.f = stdout;
  else   if (fd == fileno(stderr))
      sym->storage_as.c_file.f = stderr;
  else if (!(sym->storage_as.c_file.f = fdopen(fd ,how)))
    {
      if (close_on_error)
	close(fd);
      perror(name);
      put_st("\n");
      err("could not open file", name);
    }
  strcpy(sym->storage_as.c_file.name,name);
  no_interrupt(flag);
  return(sym);
}

LISP fopen_c(const char *name, const char *how)
{
  LISP sym;
  int fd;

  fd = fd_open_file(name, how);

  if (fd < 0)
    err("could not open file", name);

  sym = fd_to_scheme_file(fd, name, how, 1);

  return(sym);
}

LISP siod_fdopen_c(int fd, const char *name, char *how)
{
  return fd_to_scheme_file(fd, name, how, 0);
}

LISP fopen_l(LISP what, const char *r_or_w)
{
  int fd = -1;
  const char *filename = NULL;

  if (NULLP(what))
    {
      filename = "-";
      fd = fd_open_stdinout(r_or_w);
    }
  else if (SYMBOLP(what) || STRINGP(what))
    {
      fd = fd_open_file((filename = get_c_string(what)), r_or_w);
    }
  else if (LIST1P(what))
    {
      fd = fd_open_file((filename = get_c_string(CAR(what))), r_or_w);
    }
  else if (CONSP(what)  &&  !CONSP(CDR(what)))
    {
      filename = "[tcp connection]";
      fd = fd_open_url("tcp", 
		       get_c_string(CAR(what)),
		       get_c_string(CDR(what)),
		       NULL,
		       r_or_w);
    }
  else if (LIST4P(what))
    {
      filename = "[url]";
      fd = fd_open_url(get_c_string(CAR1(what)),
		       get_c_string(CAR2(what)),
		       get_c_string(CAR3(what)),
		       get_c_string(CAR4(what)),
		       r_or_w);
    }
  else
    err("not openable", what);

  if (fd<0)
    err("can't open", what);

  return fd_to_scheme_file(fd, filename, r_or_w, 1);
}

static void file_gc_free(LISP ptr)
{if ((ptr->storage_as.c_file.f) &&
     (ptr->storage_as.c_file.f != stdin) &&
     (ptr->storage_as.c_file.f != stdout))
   {fclose(ptr->storage_as.c_file.f);
    ptr->storage_as.c_file.f = (FILE *) NULL;}
 if (ptr->storage_as.c_file.name)
   {wfree(ptr->storage_as.c_file.name);
    ptr->storage_as.c_file.name = NULL;}}
   
LISP fclose_l(LISP p)
{long flag;
 flag = no_interrupt(1);
 if NTYPEP(p,tc_c_file) err("not a file",p);
 file_gc_free(p);
 open_files = delq(p,open_files);
 no_interrupt(flag);
 return(NIL);}

static void file_prin1(LISP ptr,FILE *f)
{char *name;
 name = ptr->storage_as.c_file.name;
 fput_st(f,"#<FILE ");
 sprintf(tkbuffer," %p",(void *)ptr->storage_as.c_file.f);
 fput_st(f,tkbuffer);
 if (name)
   {fput_st(f," ");
    fput_st(f,name);}
 fput_st(f,">");}

FILE *get_c_file(LISP p,FILE *deflt)
{if (NULLP(p) && deflt) return(deflt);
 if NTYPEP(p,tc_c_file) err("not a file",p);
 if (!p->storage_as.c_file.f) err("file is closed",p);
 return(p->storage_as.c_file.f);}

LISP lgetc(LISP p)
{int i;
 i = f_getc(get_c_file(p,stdin));
 return((i == EOF) ? NIL : flocons((double)i));}

LISP lputc(LISP c,LISP p)
{long flag;
 int i;
 FILE *f;
 f = get_c_file(p,stdout);
 if FLONUMP(c)
   i = (int)FLONM(c);
 else
   i = *get_c_string(c);
 flag = no_interrupt(1);
 putc(i,f);
 no_interrupt(flag);
 return(NIL);}
     
LISP lputs(LISP str,LISP p)
{fput_st(get_c_file(p,stdout),get_c_string(str));
 return(NIL);}

LISP lftell(LISP file)
{return(flocons((double)ftell(get_c_file(file,NULL))));}

LISP lfseek(LISP file,LISP offset,LISP direction)
{return((fseek(get_c_file(file,NULL),get_c_int(offset),get_c_int(direction)))
	? NIL : truth);}

static LISP directory_entries(LISP ldir, LISP lnoflagdir)
{
  LISP lentries=NIL;
  EST_Pathname dir(get_c_string(ldir));

  if (dir == "")
    return NIL;

  dir = dir.as_directory();

  EST_StrList entries(dir.entries(lnoflagdir!=NIL?0:1));
  EST_Litem *item;

  for(item=entries.head(); item; item = item->next())
    {
      EST_String entry(entries(item));
      if (entry != "../" && entry != "./" && entry != ".." && entry != ".")
	{
	  LISP litem = strintern(entry);
	  lentries = cons(litem, lentries);
	}
    }

  return lentries;
}

static LISP fopen_l(LISP what,LISP how)
{
  const char *r_or_w = NULLP(how) ? "rb" : get_c_string(how);

  return fopen_l(what, r_or_w);

}

static LISP lfread(LISP size,LISP file)
{long flag,n,ret,m;
 char *buffer;
 LISP s;
 FILE *f;
 f = get_c_file(file,NULL);
 flag = no_interrupt(1);
 if TYPEP(size,tc_string)
   {s = size;
    buffer = s->storage_as.string.data;
    n = s->storage_as.string.dim;
    m = 0;}
 else
   {n = get_c_int(size);
    buffer = (char *) must_malloc(n+1);
    buffer[n] = 0;
    m = 1;}
 ret = fread(buffer,1,n,f);
 if (ret == 0)
   {if (m)
      wfree(buffer);
    no_interrupt(flag);
    return(NIL);}
 if (m)
   {if (ret == n)
      {s = cons(NIL,NIL);
       s->type = tc_string;
       s->storage_as.string.data = buffer;
       s->storage_as.string.dim = n;}
    else
      {s = strcons(ret,NULL);
       memcpy(s->storage_as.string.data,buffer,ret);
       wfree(buffer);}
    no_interrupt(flag);
    return(s);}
 no_interrupt(flag);
 return(flocons((double)ret));}

static LISP lfwrite(LISP string,LISP file)
{FILE *f;
 long flag;
 char *data;
 long dim;
 f = get_c_file(file,NULL);
 if NTYPEP(string,tc_string) err("not a string",string);
 data = string->storage_as.string.data;
 dim = string->storage_as.string.dim;
 flag = no_interrupt(1);
 fwrite(data,dim,1,f);
 no_interrupt(flag);
 return(NIL);}

LISP lprin1f(LISP exp,FILE *f)
{LISP tmp;
 struct user_type_hooks *p;
 STACK_CHECK(&exp);
 INTERRUPT_CHECK();
 switch TYPE(exp)
   {case tc_nil:
      fput_st(f,"nil");
      break;
   case tc_cons:
      fput_st(f,"(");
      lprin1f(car(exp),f);
      for(tmp=cdr(exp);CONSP(tmp);tmp=cdr(tmp))
	{fput_st(f," ");lprin1f(car(tmp),f);}
      if NNULLP(tmp) {fput_st(f," . ");lprin1f(tmp,f);}
      fput_st(f,")");
      break;
    case tc_flonum:
      if (FLONMPNAME(exp) == NULL)
      {
	  sprintf(tkbuffer,"%.8g",FLONM(exp));
	  FLONMPNAME(exp) = (char *)must_malloc(strlen(tkbuffer)+1);
	  sprintf(FLONMPNAME(exp),"%s",tkbuffer);
      }
      sprintf(tkbuffer,"%s",FLONMPNAME(exp));
      fput_st(f,tkbuffer);
      break;
    case tc_symbol:
      fput_st(f,PNAME(exp));
      break;
    case tc_subr_0:
    case tc_subr_1:
    case tc_subr_2:
    case tc_subr_3:
    case tc_subr_4:
    case tc_lsubr:
    case tc_fsubr:
    case tc_msubr:
      sprintf(tkbuffer,"#<SUBR(%d) ",TYPE(exp));
      fput_st(f,tkbuffer);
      fput_st(f,(*exp).storage_as.subr.name);
      fput_st(f,">");
      break;
    case tc_closure:
      fput_st(f,"#<CLOSURE ");
      lprin1f(car((*exp).storage_as.closure.code),f);
      fput_st(f," ");
      lprin1f(cdr((*exp).storage_as.closure.code),f);
      fput_st(f,">");
      break;
    default:
      p = get_user_type_hooks(TYPE(exp));
      if (p->prin1)
	(*p->prin1)(exp,f);
      else 
      {
	  if (p->name) 
	      sprintf(tkbuffer,"#<%s %p>",p->name,USERVAL(exp));
	  else
	      sprintf(tkbuffer,"#<UNKNOWN %d %p>",TYPE(exp),(void *)exp);
	  fput_st(f,tkbuffer);}}
 return(NIL);}

static LISP lprintfp(LISP exp,LISP file)
{lprin1f(exp,get_c_file(file,stdout));
 return(NIL);}

static LISP terpri(LISP file)
{fput_st(get_c_file(file,stdout),"\n");
 return(NIL);}

static LISP lreadfp(LISP file)
{return lreadf(get_c_file(file,stdout));}

LISP load(LISP fname,LISP cflag)
{return(vload(get_c_string(fname),NULLP(cflag) ? 0 : 1));}

LISP lprint(LISP exp)
{lprin1f(exp,stdout);
 put_st("\n");
 return(NIL);}

LISP lread(void)
{return(lreadf(stdin));}

LISP get_eof_val(void)
{return(eof_val);}

static LISP probe_file(LISP fname)
{
    // return t if file exists, nil otherwise
    const char *filename;

    filename = get_c_string(fname);
    if (access(filename,R_OK) == 0)
	return truth;
    else
	return NIL;
}

static LISP lunlink(LISP name)
{
    unlink(get_c_string(name));
    return NIL;
}

static LISP save_forms(LISP fname,LISP forms,LISP how)
{const char *cname;
 const char *chow = NULL;
 LISP l,lf;
 FILE *f;
 cname = get_c_string(fname);
 if EQ(how,NIL) chow = "wb";
 else if EQ(how,cintern("a")) chow = "a";
 else err("bad argument to save-forms",how);
 fput_st(fwarn,(*chow == 'a') ? "appending" : "saving");
 fput_st(fwarn," forms to ");
 fput_st(fwarn,cname);
 fput_st(fwarn,"\n");
 lf = fopen_c(cname,chow);
 f = lf->storage_as.c_file.f;
 for(l=forms;NNULLP(l);l=cdr(l))
   {lprin1f(car(l),f);
    putc('\n',f);}
 fclose_l(lf);
 fput_st(fwarn,"done.\n");
 return(truth);}

void close_open_files_upto(LISP end)
{LISP l,p;
 for(l=open_files;((l!=end)&&(l!=NIL));l=cdr(l))
   {p = car(l);
    if (p->storage_as.c_file.f)
      {fprintf(stderr,"closing a file left open: %s\n",
	       (p->storage_as.c_file.name) ? p->storage_as.c_file.name : "");
       fflush(stderr);
       file_gc_free(p);}}
 open_files = l;}

void close_open_files(void)
{
    close_open_files_upto(NIL);
}

static void check_first_line(FILE *lf)
{  /* If this line starts #! skip it -- this is for scripts */
    int c0,c1,c2;
    if ((c0=getc(lf)) == '#')
    {
	if ((c1 = getc(lf)) == '!')
	    while (((c2=getc(lf)) != '\n') && (c2 != EOF)); /* skip to EOLN */
	else
	{
	    ungetc(c1,lf);
	    ungetc(c0,lf);  /* possibly something don't support 2 ungets */
	}
    }
    else
	ungetc(c0,lf);
}

LISP vload(const char *fname_raw, long cflag)
{
  LISP form,result,tail,lf;
 FILE *f;
  EST_Pathname fname(fname_raw);
 fput_st(fwarn,"loading ");
 fput_st(fwarn,fname);
 fput_st(fwarn,"\n");
 lf = fopen_c(fname,"rb");
 f = lf->storage_as.c_file.f;
 if (!cflag)
     check_first_line(f);
 result = NIL;
 tail = NIL;
 while(1)
   {form = lreadf(f);
    if EQ(form,eof_val) break;
    if (cflag)
      {form = cons(form,NIL);
       if NULLP(result)
	 result = tail = form;
       else
	 tail = setcdr(tail,form);}
    else
      leval(form,NIL);}
 fclose_l(lf);
 fput_st(fwarn,"done.\n");
 return(result);}

void init_subrs_file(void)
{
    long j;
    set_gc_hooks(tc_c_file,FALSE,NULL,NULL,NULL,file_gc_free,NULL,&j);
    set_print_hooks(tc_c_file,file_prin1, NULL);
    setvar(cintern("stderr"),
	   fd_to_scheme_file(fileno(stderr),"stderr","w",FALSE),NIL);

 init_subr_2("fread",lfread,
  "(fread BUFFER FILE)\n\
  BUFFER is a string of length N, N bytes are read from FILE into\n\
  BUFFER.");
 init_subr_2("fwrite",lfwrite,
  "(fwrite BUFFER FILE)\n\
  Write BUFFER into FILE.");

 init_subr_0("read",lread,
 "(read)\n\
  Read next s-expression from stdin and return it.");
 init_subr_0("eof-val",get_eof_val,
 "(eof_val)\n\
  Returns symbol used to indicate end of file.  May be used (with eq?)\n\
  to determine when end of file occurs while reading files.");
 init_subr_1("print",lprint,
 "(print DATA)\n\
  Print DATA to stdout if textual form.  Not a pretty printer.");
 init_subr_2("pprintf",siod_pprintf,
 "(pprintf EXP [FD])\n\
 Pretty print EXP to FD, if FD is nil print to the screen.");
 init_subr_2("printfp",lprintfp,
 "(printfp DATA FILEP)\n\
  Print DATA to file indicated by file pointer FILEP.  File pointers are\n\
  are created by fopen.");
 init_subr_1("readfp",lreadfp,
 "(readfp FILEP)\n\
  Read and return next s-expression from file indicated by file pointer\n\
  FILEP.  File pointers are created by fopen.");
 init_subr_1("terpri",terpri,
 "(terpri FILEP)\n\
  Print newline to FILEP, is FILEP is nil or not specified a newline it\n\
  is printed to stdout.");
 init_subr_1("fflush",fflush_l,
 "(fflush FILEP)\n\
  Flush FILEP. If FILEP is nil, then flush stdout.");
 init_subr_2("fopen",fopen_l,
 "(fopen FILENAME HOW)\n\
  Return file pointer for FILENAME opened in mode HOW.");
 init_subr_1("fclose",fclose_l,
 "(fclose FILEP)\n\
  Close filepoint FILEP.");
 init_subr_1("getc",lgetc,
 "(getc FILEP)\n\
  Get next character from FILEP.  Character is returned as a number. If\n\
  FILEP is nil, or not specified input comes from stdin.");
 init_subr_2("putc",lputc,
 "(putc ECHAR FILEP)\n\
  Put ECHAR (a number) as a character to FILEP.  If FILEP is nil or not\n\
  specified output goes to stdout.");
 init_subr_2("puts",lputs,
 "(puts STRING FILEP)\n\
  Write STRING (print name of symbol) to FILEP.  If FILEP is nil or not\n\
  specified output goes to stdout.");
 init_subr_1("ftell",lftell,
 "(ftell FILEP)\n\
  Returns position in file FILEP is currently pointing at.");
 init_subr_3("fseek",lfseek,
 "(fseek FILEP OFFSET DIRECTION)\n\
  Position FILEP to OFFSET. If DIRECTION is 0 offset is from start of file.\n\
  If DIRECTION is 1, offset is from current position.  If DIRECTION is\n\
  2 offset is from end of file.");
 init_subr_1("probe_file",probe_file,
 "(probe_file FILENAME)\n\
  Returns t if FILENAME exists and is readable, nil otherwise.");
 init_subr_1("delete-file",lunlink,
 "(delete-file FILENAME)\n\
  Delete named file.");
 init_subr_2("load",load,
 "(load FILENAME OPTION)\n\
  Load s-expressions in FILENAME.  If OPTION is nil or unspecified evaluate\n\
  each s-expression in FILENAME as it is read, if OPTION is t, return them\n\
  unevaluated in a list.");

 init_subr_2("directory-entries", directory_entries,
 "(directory-entries DIRECTORY &opt NOFLAGDIR)\n\
  Return a list of the entries in the directory. If NOFLAGDIR is non-null\n\
  don't check to see which are directories.");

 init_subr_3("save-forms",save_forms,
 "(save-forms FILENAME FORMS HOW)\n\
  Save FORMS in FILENAME.  If HOW is a appending FORMS to FILENAME,\n\
  or if HOW is w start from the beginning of FILENAME.");
}
