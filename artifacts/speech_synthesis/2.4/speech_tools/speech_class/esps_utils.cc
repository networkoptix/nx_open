/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                   Copyright (c) 1994,1995,1996                        */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                    Author :  Alan W Black (and Paul Taylor)           */
/*                    Date   :  June 1996                                */
/*-----------------------------------------------------------------------*/
/*  These routines form a basis for reading and writing Entropic's ESPS  */
/*  headered files.  The reason we wrote them was to avoid including     */
/*  Entropic's own (large and cumbersome) code into all our programs.    */
/*  No Entropic proprietary code is included in this code which means    */
/*  you do not needs an Entropic licence to use it.                      */
/*                                                                       */
/*  However this should not be seen as anti-Entropic in anyway it is for */
/*  our and your convenience.  We would like to specifically thank       */
/*  Rodney Johnson of Entropic for giving us help and confirming to us   */
/*  the header format is in fact more complex than one can imagine,      */
/*  mostly for very bad reasons, (backward compatibility cripples all    */
/*  software in the long run).  Hence this code is almost definitely     */
/*  incomplete and is not guaranteed to read or create all ESPS files    */
/*  properly but it is adequate for many tasks.                          */
/*                                                                       */
/*  Also thanks go to Peter Kabal from McGill University whose AF code   */
/*  showed me this might be worth attempting, his code gave me something */
/*  to look at to start with.                                            */
/*                                                                       */
/*  I should add, this wasn't easy to write, though I'm much better at   */
/*  octal and hex dumps now.                                             */
/*=======================================================================*/

#include <cstdio>
#include <cstdlib>
#include "EST_unix.h"
#include <cstring>
#include <time.h>
#include "EST_wave_utils.h"
#include "esps_utils.h"

/* First you must realise there is in fact a number of very similar but */
/* subtly different header formats that appear on ESPS files.           */
/* ESPS_FEA and ESPS_SD (others for filters, spectrograms, etc)         */
/* The basic format is                                                  */
/*     preamble                                                         */
/*     fixed header                                                     */
/*     variable header                                                  */
/*        field descriptions (type and dimensions)                      */
/*        field names                                                   */
/*        header FEAs (maybe with values)                               */
/*        old and foreign headers (mostly ignored here)                 */
/*     data records themselves                                          */

/* esps_fea contain a name and possibly a value.  They appear in the    */
/* variable part of the header                                          */
esps_fea new_esps_fea()
{
    esps_fea r = walloc(struct ESPS_FEA_struct,1);
    r->type = 0;
    r->clength = 0;
    r->name = NULL;
    r->dtype = 0;
    r->count = 0;
    r->v.ival = NULL;
    return r;
}

void delete_esps_fea(esps_fea r)
{
    esps_fea t,u;

    for (t=r; t != NULL; t=u)
    {
	if (t->clength != 0)
	    wfree(t->name);
	if (t->count != 0) /* this wont work if fields in v aren't aligned */
	    wfree(t->v.ival);
	u = t->next;
	wfree(t);
    }
}

void print_esps_fea(esps_fea r)
{
    /* Print out the information in the FEA record */
    int i;
    
    fprintf(stdout,"type:  %d\n",r->type);
    fprintf(stdout,"name:  %s\n",r->name);
    fprintf(stdout,"size:  %d\n",r->count);
    fprintf(stdout,"dtype: %d\n",r->dtype);
    for (i=0; i<r->count; i++)
	switch (r->dtype)
	{
	  case ESPS_DOUBLE:
	    fprintf(stdout," %d: %g\n",i,r->v.dval[i]); break;
	  case ESPS_FLOAT:
	    fprintf(stdout," %d: %f\n",i,r->v.fval[i]); break;
	  case ESPS_INT:
	    fprintf(stdout," %d: %d\n",i,r->v.ival[i]); break;
	  case ESPS_SHORT:
	    fprintf(stdout," %d: %d\n",i,r->v.sval[i]); break;
	  case ESPS_CHAR:
	    fprintf(stdout," %d: %d\n",i,r->v.cval[i]); break;
	  default:
	    fprintf(stdout," %d: unknown\n",i);
	}
}

void add_field(esps_hdr hdr, const char *name, int type, int dimension)
{
    /* Add a new field to the record */
    char **names = hdr->field_name;
    short *types = hdr->field_type;
    int *dims = hdr->field_dimension;
    int i;

    hdr->field_name = walloc(char *,hdr->num_fields+1);
    hdr->field_type = walloc(short,hdr->num_fields+1);
    hdr->field_dimension = walloc(int,hdr->num_fields+1);
    for (i=0; i < hdr->num_fields; i++)
    {
	hdr->field_name[i] = names[i];
	hdr->field_type[i] = types[i];
	hdr->field_dimension[i] = dims[i];
    }
    wfree(names);
    wfree(types);
    wfree(dims);
    hdr->field_name[hdr->num_fields] = wstrdup(name);
    hdr->field_type[hdr->num_fields] = type;
    hdr->field_dimension[hdr->num_fields] = dimension;
    hdr->num_fields++;

    return;

}

void add_fea_d(esps_hdr hdr,const char *name, int pos, double d)
{
    /* Add a double FEA field to the header */
    esps_fea t = new_esps_fea();
    int i;

    t->type = 13;  /* must be lucky for some !! */
    t->clength = strlen(name);
    t->name = wstrdup(name);
    if (t->count < pos+1)
    {
	double *dval = t->v.dval;
	t->v.dval = walloc(double,pos+1);
	for (i=0; i<t->count; i++)
	    t->v.dval[i] = dval[i];
	for (; i < pos+1; i++)
	    t->v.dval[i] = 0.0;
	wfree(dval);
	t->count = pos+1;
    }
    t->dtype = ESPS_DOUBLE;
    t->v.dval[pos] = d;

    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

void add_fea_f(esps_hdr hdr,const char *name, int pos, float d)
{
    /* Add a float FEA field to the header */
    esps_fea t = new_esps_fea();
    int i;

    t->type = 13;
    t->clength = strlen(name);
    t->name = wstrdup(name);
    if (t->count < pos+1)
    {
	float *fval = t->v.fval;
	t->v.fval = walloc(float,pos+1);
	for (i=0; i<t->count; i++)
	    t->v.fval[i] = fval[i];
	for (; i < pos+1; i++)
	    t->v.fval[i] = 0.0;
	wfree(fval);
	t->count = pos+1;
    }
    t->dtype = ESPS_FLOAT;
    t->v.fval[pos] = d;

    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

void add_fea_i(esps_hdr hdr,const char *name, int pos, int d)
{
    /* Add an int FEA field to the header */
    esps_fea t = new_esps_fea();
    int i;

    t->type = 13;
    t->clength = strlen(name);
    t->name = wstrdup(name);
    if (t->count < pos+1)
    {
	int *ival = t->v.ival;
	t->v.ival = walloc(int,pos+1);
	for (i=0; i<t->count; i++)
	    t->v.ival[i] = ival[i];
	for (; i < pos+1; i++)
	    t->v.ival[i] = 0;
	wfree(ival);
	t->count = pos+1;
    }
    t->dtype = ESPS_INT;
    t->v.ival[pos] = d;

    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

void add_fea_s(esps_hdr hdr,const char *name, int pos, short d)
{
    /* Add a short FEA field to the header */
    esps_fea t = new_esps_fea();
    int i;

    t->type = 13;
    t->clength = strlen(name);
    t->name = wstrdup(name);
    if (t->count < pos+1)
    {
	short *sval = t->v.sval;
	t->v.sval = walloc(short,pos+1);
	for (i=0; i<t->count; i++)
	    t->v.sval[i] = sval[i];
	for (; i < pos+1; i++)
	    t->v.sval[i] = (short)0;
	wfree(sval);
	t->count = pos+1;
    }
    t->dtype = ESPS_SHORT;
    t->v.sval[pos] = d;

    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

void add_fea_c(esps_hdr hdr,const char *name, int pos, char d)
{
    /* Add a char FEA field to the header */
    esps_fea t = new_esps_fea();
    int i;

    t->type = 13;
    t->clength = strlen(name);
    t->name = wstrdup(name);
    if (t->count < pos+1)
    {
	char *cval = t->v.cval;
	t->v.cval = walloc(char,pos+1);
	for (i=0; i<t->count; i++)
	    t->v.cval[i] = cval[i];
	for (; i < pos+1; i++)
	    t->v.cval[i] = (char)0;
	wfree(cval);
	t->count = pos+1;
    }
    t->dtype = ESPS_CHAR;
    t->v.cval[pos] = d;

    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

void add_fea_special(esps_hdr hdr,int type,const char *name)
{
    /* Add a special FEA field to the header */
    esps_fea t = new_esps_fea();

    t->type = type;
    t->clength = strlen(name);
    t->name = wstrdup(name);
    t->count = 0;
    
    t->next = hdr->fea;
    hdr->fea = t;

    return;
}

int fea_value_d(const char *name,int pos,esps_hdr hdr, double *d)
{
    /* Get value of double FEA */
    esps_fea t;
    
    for (t=hdr->fea; t != NULL; t=t->next)
	if (streq(name,t->name))
	{
	    if (t->dtype != ESPS_DOUBLE)
	    {
		fprintf(stderr,"ESPS hdr: access non-double field \"%s\" as double\n",
			name);
		return -1;
	    }
	    *d = t->v.dval[pos];
	    return 0;
	}

    return -1;  /* failed to find it */
}

int fea_value_f(const char *name,int pos,esps_hdr hdr, float *d)
{
    /* Get value of float FEA */
    esps_fea t;
    
    for (t=hdr->fea; t != NULL; t=t->next)
	if (streq(name,t->name))
	{
	    if (t->dtype != ESPS_FLOAT)
	    {
		fprintf(stderr,"ESPS hdr: access non-float field \"%s\" as float\n",
			name);
		return -1;
	    }
	    *d = t->v.fval[pos];
	    return 0;
	}

    return -1;  /* failed to find it */
}

int fea_value_s(const char *name,int pos,esps_hdr hdr, short *d)
{
    /* Get value of short FEA */
    esps_fea t;
    
    for (t=hdr->fea; t != NULL; t=t->next)
	if (streq(name,t->name))
	{
	    if (t->dtype != ESPS_SHORT)
	    {
		fprintf(stderr,"ESPS hdr: access non-short field \"%s\" as short\n",
			name);
		return -1;
	    }
	    *d = t->v.sval[pos];
	    return 0;
	}

    return -1;  /* failed to find it */
}

int fea_value_i(const char *name,int pos,esps_hdr hdr, int *d)
{
    /* Get value of int FEA */
    esps_fea t;
    
    for (t=hdr->fea; t != NULL; t=t->next)
	if (streq(name,t->name))
	{
	    if (t->dtype != ESPS_INT)
	    {
		fprintf(stderr,"ESPS hdr: access non-int field \"%s\" as int\n",
			name);
		return -1;
	    }
	    *d = t->v.ival[pos];
	    return 0;
	}

    return -1;  /* failed to find it */
}

int fea_value_c(const char *name,int pos,esps_hdr hdr, char *d)
{
    /* Get value of int FEA */
    esps_fea t;
    
    for (t=hdr->fea; t != NULL; t=t->next)
	if (streq(name,t->name))
	{
	    if (t->dtype != ESPS_CHAR)
	    {
		fprintf(stderr,"ESPS hdr: access non-char field \"%s\" as char\n",
			name);
		return -1;
	    }
	    *d = t->v.cval[pos];
	    return 0;
	}

    return -1;  /* failed to find it */
}

static int esps_alloc_fea(esps_fea r)
{
    switch (r->dtype)
    {
      case 0: /* nothing */
	break;
      case ESPS_DOUBLE:
	r->v.dval = walloc(double,r->count); break;
      case ESPS_FLOAT:
	r->v.fval = walloc(float,r->count); break;
      case ESPS_INT:
	r->v.ival = walloc(int,r->count); break;
      case ESPS_SHORT:
	r->v.sval = walloc(short,r->count); break;
      case ESPS_CHAR:
	r->v.cval = walloc(char,r->count); break;
      default:
	fprintf(stderr,"ESPS file: unsupported FEA dtype\n");
	return -1;
    }

    return 0;
}

void write_esps_fea(FILE *fd, esps_fea t,esps_hdr hdr)
{
    /* write out this fea */
    (void)hdr;
    short clength;
    char *nspace;
    int i;

    fwrite(&t->type,2,1,fd);
    clength = (strlen(t->name)+3)/4;
    fwrite(&clength,2,1,fd);
    nspace = walloc(char, clength*4);
    memset(nspace,0,clength*4);
    memmove(nspace,t->name,strlen(t->name));
    fwrite(nspace,1,clength*4,fd);
    wfree(nspace);
    if ((t->type == 11) ||
	(t->type == 1) ||
	(t->type == 15))
	return;
    fwrite(&t->count,4,1,fd);
    fwrite(&t->dtype,2,1,fd);
    
    for (i=0; i<t->count; i++)
    {
	switch(t->dtype)
	{
	  case ESPS_DOUBLE: 
	    fwrite(&t->v.dval[i],8,1,fd); break;
	  case ESPS_FLOAT: 
	    fwrite(&t->v.fval[i],4,1,fd); break;
	  case ESPS_INT: 
	    fwrite(&t->v.ival[i],4,1,fd); break;
	  case ESPS_SHORT: 
	    fwrite(&t->v.sval[i],2,1,fd); break;
	  case ESPS_CHAR: 
	    fwrite(&t->v.cval[i],1,1,fd); break;
	  default:
	    fprintf(stderr,"ESPS write_hdr: unsupported FEA dtype %d\n",
		    t->dtype);
	}
    }
    return;
}

int write_esps_rec(esps_rec r, esps_hdr h, FILE *fd)
{
    /* will have to worry about swap someday */
    (void)h;
    int i;

    for (i=0; i < r->num_fields; i++)
    {
	switch(r->field[i]->type)
	{
	  case ESPS_DOUBLE:
	    fwrite(r->field[i]->v.dval,8,r->field[i]->dimension,fd);
	    break;
	  case ESPS_FLOAT:
	    fwrite(r->field[i]->v.fval,4,r->field[i]->dimension,fd);
	    break;
	  case ESPS_INT:
	    fwrite(r->field[i]->v.ival,4,r->field[i]->dimension,fd);
	    break;
	  case ESPS_SHORT:
	    fwrite(r->field[i]->v.sval,2,r->field[i]->dimension,fd);
	    break;
	  case ESPS_CHAR:
	    fwrite(r->field[i]->v.cval,1,r->field[i]->dimension,fd);
	    break;
	  case ESPS_CODED:
	    fwrite(r->field[i]->v.sval,2,r->field[i]->dimension,fd);
	    break;

	  default:
	    fprintf(stderr,"ESPS file: unsupported field type %d\n",
		    r->field[i]->type);
	}
    }
    return 0;
}

esps_fea read_esps_fea(FILE *fd, esps_hdr hdr)
{
    /* read next FEA record at point */
    esps_fea r = new_esps_fea();
    short sdata;
    int i;
    int idata;
    float fdata;
    double ddata;
    char cdata;

    fread(&sdata,2,1,fd);
    if (hdr->swapped) sdata = SWAPSHORT(sdata);
    r->type = sdata;
    if (r->type == 0)              /* a field name */
    {   /* next short is the size in bytes */
	fread(&sdata,2,1,fd);
	if (hdr->swapped) sdata = SWAPSHORT(sdata);
	r->clength = sdata;
    }
    else if ((r->type == 13) ||   /* a feature and value */
             (r->type == 11) ||   /* a single string (comment ?) */
             (r->type == 1)  ||   /* a filename */
             (r->type == 4)  ||   /* a filename */
	     (r->type == 15))     /* directory name */
    {                 
	fread(&sdata,2,1,fd);
	if (hdr->swapped) sdata = SWAPSHORT(sdata);
	r->clength = sdata * 4;
    }
    else
    {
	fprintf(stderr,"ESPS: fea record unknown type\n");
	wfree(r);
	return NULL;
    }
    r->name = walloc(char,r->clength+1);
    fread(r->name,1,r->clength,fd);
    r->name[r->clength] = '\0';
    if ((r->type == 11) ||       /* a single string */
	(r->type == 1)  ||       /* a filename */
	(r->type == 15))         /* directory name */
	return r;  
    fread(&idata,4,1,fd);
    if (hdr->swapped) idata = SWAPINT(idata);
    r->count = idata;
    fread(&sdata,2,1,fd);
    if (hdr->swapped) sdata = SWAPSHORT(sdata);
    r->dtype = sdata;
    if (esps_alloc_fea(r) == -1)
	return NULL;
    for (i=0; i<r->count; i++)
    {
	switch (r->dtype)
	{
	  case ESPS_DOUBLE:
	    fread(&ddata,8,1,fd);
	    if (hdr->swapped) swapdouble(&ddata);
	    r->v.dval[i] = ddata;
	    break;
	  case ESPS_FLOAT:
	    fread(&fdata,4,1,fd);
	    if (hdr->swapped) swapfloat(&fdata);
	    r->v.fval[i] = fdata;
	    break;
	  case ESPS_INT:
	    fread(&idata,4,1,fd);
	    if (hdr->swapped) idata = SWAPINT(idata);
	    r->v.ival[i] = idata;
	    break;
	  case ESPS_SHORT:
	    fread(&sdata,2,1,fd);
	    if (hdr->swapped) sdata = SWAPSHORT(sdata);
	    r->v.sval[i] = sdata;
	    break;
	  case ESPS_CHAR:
	    fread(&cdata,1,1,fd);
	    r->v.cval[i] = cdata;
	    break;
	  default:
	    fprintf(stderr,"ESPS read_hdr: unsupported FEA dtype %d\n",r->dtype);
	    wfree(r);
	    return NULL;
	}
    }

    return r;
}    

static char *esps_get_field_name(FILE *fd, esps_hdr hdr, int expect_source)
{
    /* read the next field name */
  short size=0;  /* bet its really a short */
  char *name;
  
  if (fread(&size,2,1,fd) != 1)
    {
      fputs("error reading field name size\n", stderr);
      return wstrdup("ERROR");
    }
  if (hdr->swapped) size = SWAPSHORT(size);
  name = walloc(char,size+1);
  if (fread(name,1,size,fd) != (unsigned)size)
    {
      fputs("error reading field name\n", stderr);
      strncpy(name, "ERROR", size);
    }
  name[size] = '\0';
  if (hdr->file_type == ESPS_SD || expect_source)
    fseek(fd,6,SEEK_CUR);  /* skip some zeroes */
  else
    fseek(fd,2,SEEK_CUR);

  if (expect_source)
    {
      fread(&size,2,1,fd);
      if (hdr->swapped) size = SWAPSHORT(size);
      fseek(fd,size,SEEK_CUR);
    }

  return name;
}

static void esps_put_field_name(char *name,FILE *fd, esps_hdr hdr)
{
    /* write the next field name */
    short size = strlen(name);
    short shortdata;

    shortdata = 0;
    fwrite(&shortdata,2,1,fd);
    fwrite(&size,2,1,fd);
    fwrite(name,1,size,fd);
    if (hdr->file_type == ESPS_SD)
    {
	shortdata = 0;
	fwrite(&shortdata,2,1,fd);
	fwrite(&shortdata,2,1,fd);
	fwrite(&shortdata,2,1,fd);
    }
    return;
}

esps_hdr new_esps_hdr(void)
{
    esps_hdr h = walloc(struct ESPS_HDR_struct,1);
    h->file_type = ESPS_FEA;
    h->swapped = FALSE;
    h->num_records = 0;
    h->num_fields = 0;
    h->field_name = NULL;
    h->field_type = NULL;
    h->field_dimension = NULL;
    h->fea = NULL;
    return h;
}

void delete_esps_hdr(esps_hdr h)
{
    int i;
    if (h != NULL)
    {
	if (h->field_name != NULL)
	{
	    for (i=0; i < h->num_fields; i++)
		wfree(h->field_name[i]);
	    wfree(h->field_name);
	}
	delete_esps_fea(h->fea);
    }
}

esps_rec new_esps_rec(esps_hdr hdr)
{
    /* New esps record */
    esps_rec r = walloc(struct ESPS_REC_struct,1);
    int i,size;

    r->field = walloc(esps_field,hdr->num_fields);
    for (size=0,i=0; i < hdr->num_fields; i++)
    {
	r->field[i]=walloc(struct ESPS_FIELD_struct,1);
	r->field[i]->type = hdr->field_type[i];
	r->field[i]->dimension = hdr->field_dimension[i];
	switch(r->field[i]->type)
	{
	  case ESPS_DOUBLE:
	    r->field[i]->v.dval = walloc(double,r->field[i]->dimension);
	    size += 8;
	    break;
	  case ESPS_FLOAT:
	    r->field[i]->v.fval = walloc(float,r->field[i]->dimension);
	    size += 4;
	    break;
	  case ESPS_INT:
	    r->field[i]->v.ival = walloc(int,r->field[i]->dimension);
	    size += 4;
	    break;
	  case ESPS_SHORT:
	    r->field[i]->v.sval = walloc(short,r->field[i]->dimension);
	    size += 2;
	    break;
	  case ESPS_CHAR:
	    r->field[i]->v.cval = walloc(char,r->field[i]->dimension);
	    size += 1;
	    break;
	  case ESPS_CODED:
	    r->field[i]->v.sval = walloc(short,r->field[i]->dimension);
	    size += 2;
	    break;
	  default:
	    fprintf(stderr,"ESPS file: unsupported field type %d\n",
		    r->field[i]->type);
	}
    }
    r->num_fields = hdr->num_fields;
    r->size = size;
    return r;

}

void delete_esps_rec(esps_rec r)
{
    int i;

    for (i=0; i<r->num_fields; i++)
    {
	wfree(r->field[i]->v.ival);
	wfree(r->field[i]);
    }
    wfree(r->field);
    return;
}

int read_esps_rec(esps_rec r, esps_hdr hdr, FILE *fd)
{
    /* read the next record at point */
    int i,j;
    double doubledata;
    float floatdata;
    int intdata;
    short shortdata;
    
    for (i=0; i< r->num_fields; i++)
    {
	switch (r->field[i]->type)
	{
	  case ESPS_DOUBLE:
	    for(j=0; j < r->field[i]->dimension; j++)
	    {
		if (fread(&doubledata,8,1,fd) == 0) return EOF;
		if (hdr->swapped) swapdouble(&doubledata);
		r->field[i]->v.dval[j] = doubledata;
	    }
	    break;
	  case ESPS_FLOAT:
	    for(j=0; j < r->field[i]->dimension; j++)
	    {
		if (fread(&floatdata,4,1,fd) == 0) return EOF;
		if (hdr->swapped) swapfloat(&floatdata);
		r->field[i]->v.fval[j] = floatdata;
	    }
	    break;
	  case ESPS_INT:
	    for(j=0; j < r->field[i]->dimension; j++)
	    {
		if (fread(&intdata,4,1,fd) == 0) return EOF;
		if (hdr->swapped) intdata = SWAPINT(intdata);
		r->field[i]->v.ival[j] = intdata;
	    }
	    break;
	  case ESPS_SHORT:
	    for(j=0; j < r->field[i]->dimension; j++)
	    {
		if (fread(&shortdata,2,1,fd) == 0) return EOF;
		if (hdr->swapped) shortdata = SWAPSHORT(shortdata);
		r->field[i]->v.sval[j] = shortdata;
	    }
	    break;
	  case ESPS_CHAR:
	    if (fread(r->field[i]->v.cval,1,r->field[i]->dimension,fd) !=
		(unsigned)r->field[i]->dimension) return EOF;
	    break;
	  case ESPS_CODED:
	    for(j=0; j < r->field[i]->dimension; j++)
	    {
		if (fread(&shortdata,2,1,fd) == 0) return EOF;
		if (hdr->swapped) shortdata = SWAPSHORT(shortdata);
		r->field[i]->v.sval[j] = shortdata;
	    }
	    break;
	  default:
	    fprintf(stderr,"ESPS file: unsupported field type %d\n",
		    r->field[i]->type);
	    return EOF;
	}

    }
    
    return 0;

}

double get_field_d(esps_rec r, int field, int pos)
{
    return r->field[field]->v.dval[pos];
}
float get_field_f(esps_rec r, int field, int pos)
{
    return r->field[field]->v.fval[pos];
}
int get_field_i(esps_rec r, int field, int pos)
{
    return r->field[field]->v.ival[pos];
}
short get_field_s(esps_rec r, int field, int pos)
{
    return r->field[field]->v.sval[pos];
}
char get_field_c(esps_rec r, int field, int pos)
{
    return r->field[field]->v.cval[pos];
}
void set_field_d(esps_rec r, int field, int pos, double d)
{
    r->field[field]->v.dval[pos] = d;
}
void set_field_f(esps_rec r, int field, int pos, float d)
{
    r->field[field]->v.fval[pos] = d;
}
void set_field_i(esps_rec r, int field, int pos, int d)
{
    r->field[field]->v.ival[pos] = d;
}
void set_field_s(esps_rec r, int field, int pos, short d)
{
    r->field[field]->v.sval[pos] = d;
}
void set_field_c(esps_rec r, int field, int pos, char d)
{
    r->field[field]->v.cval[pos] = d;
}

int esps_record_size(esps_hdr hdr)
{
    /* works out the number of bytes in a record */
    esps_rec r = new_esps_rec(hdr);
    int size = r->size;
    delete_esps_rec(r);

    return size;
}

static int esps_num_of_type(int type,esps_hdr hdr)
{
    /* counts up the number of occurrences of fields of type in a record */
    int i;
    int sum;

    for (sum=i=0; i < hdr->num_fields; i++)
	if (hdr->field_type[i] == type)
	    sum++;
    return sum;
}    


esps_hdr make_esps_sd_hdr(void)
{
    /* returns a basic header for an ESPS_SD file */
    esps_hdr hdr = new_esps_hdr();
    hdr->file_type = ESPS_SD;
    return hdr;

}

esps_hdr make_esps_hdr(void)
{
    /* returns a basic header for an ESPS_SD file */
    esps_hdr hdr = new_esps_hdr();
    hdr->file_type = ESPS_FEA;
    return hdr;

}

enum EST_read_status read_esps_hdr(esps_hdr *uhdr,FILE *fd)
{
    /* reads an ESPS header from fd at point (should be position 0) */
    /* leaves point at start of data (immediately after header)     */
    struct ESPS_PREAMBLE preamble;
    struct ESPS_FIXED_HDR fhdr;
    esps_hdr hdr;
    int end,pos,intdata,i;
    short shortdata;
    double sd_sample_rate;
    int typematch;
    int swap;
    short name_flag;

    fread(&preamble,sizeof(preamble),1,fd);
    if (preamble.check == ESPS_MAGIC)
	swap = FALSE;
    else if (preamble.check == SWAPINT(ESPS_MAGIC))
	swap = TRUE;
    else
	return wrong_format;

    hdr = new_esps_hdr();
    hdr->swapped = swap;
    fread(&fhdr,sizeof(fhdr),1,fd);
    if (hdr->swapped) 
    {
	preamble.data_offset = SWAPINT(preamble.data_offset);
	preamble.record_size = SWAPINT(preamble.record_size);
	fhdr.num_samples = SWAPINT(fhdr.num_samples);
	fhdr.num_doubles = SWAPINT(fhdr.num_doubles);
	fhdr.num_floats = SWAPINT(fhdr.num_floats);
	fhdr.num_ints = SWAPINT(fhdr.num_ints);
	fhdr.num_shorts = SWAPINT(fhdr.num_shorts);
	fhdr.num_chars = SWAPINT(fhdr.num_chars);
	fhdr.fea_type = SWAPSHORT(fhdr.fea_type);
	fhdr.num_fields = SWAPSHORT(fhdr.num_fields);
    }
    pos = ftell(fd);
    if (fhdr.num_samples == 0)  /* has to be derived from the file size */
    {
	pos = ftell(fd);
	fseek(fd,0,SEEK_END);
	end = ftell(fd);
	fseek(fd,pos,SEEK_SET);
	fhdr.num_samples = (end - preamble.data_offset)/preamble.record_size;
    }
    hdr->num_records = fhdr.num_samples;
    hdr->num_fields = fhdr.num_fields;
    hdr->hdr_size = preamble.data_offset;
    if (fhdr.thirteen == 9)
    {   /* esps identifies such files are as Sample Data Files */
	hdr->file_type = ESPS_SD;
	/* fake the rest to make it appear like other SD files */
	hdr->num_fields = 1;
	hdr->field_dimension = walloc(int,hdr->num_fields);
	hdr->field_dimension[0] = 1;
	hdr->field_type = walloc(short,hdr->num_fields);
	hdr->field_type[0] = ESPS_SHORT;
	hdr->field_name = walloc(char *,1);
	hdr->field_name[0] = wstrdup("samples");
	fseek(fd,hdr->hdr_size,SEEK_SET);
	/* In this cases its just in the header as a float */
	sd_sample_rate = *((float *)&fhdr.fil4[0]);
	add_fea_d(hdr,"record_freq",0,(double)sd_sample_rate);
	*uhdr = hdr;
	return format_ok;
    }
    else if ((fhdr.fea_type == 8) &&
	     (hdr->num_fields == 1) &&
	     ((fhdr.num_shorts*2) == preamble.record_size))
	hdr->file_type = ESPS_SD;  /* this is a heuristic */
    else
	hdr->file_type = ESPS_FEA;
    /* Now we have the field descriptions */
    
    /* 0000 0001 dimensions */
    hdr->field_dimension = walloc(int,hdr->num_fields);
    for (i=0; i<hdr->num_fields; i++)                   
    {
	fread(&intdata,4,1,fd);                         /* dimensions */
	if (hdr->swapped) intdata = SWAPINT(intdata);
	hdr->field_dimension[i] = intdata;
    }
    /* 0 -> num_fields-1 -- probably ordering information */
    fseek(fd,hdr->num_fields*4,SEEK_CUR);               /* ordering info */
    fseek(fd,hdr->num_fields*2,SEEK_CUR);               /* zeros */
    hdr->field_type = walloc(short,hdr->num_fields);    
    for (i=0; i<hdr->num_fields; i++)
    {
	fread(&shortdata,2,1,fd);                       /* field types */  
	if (hdr->swapped) shortdata = SWAPSHORT(shortdata);
	hdr->field_type[i] = shortdata;
    }
    typematch = TRUE;
    fread(&intdata,4,1,fd);                             /* number of doubles */
    if (hdr->swapped) intdata = SWAPINT(intdata);
    if (fhdr.num_doubles != intdata) typematch = FALSE;
    fread(&intdata,4,1,fd);                             /* number of floats */
    if (hdr->swapped) intdata = SWAPINT(intdata);
    if (fhdr.num_floats != intdata) typematch = FALSE;
    fread(&intdata,4,1,fd);                             /* number of ints */
    if (hdr->swapped) intdata = SWAPINT(intdata);
    if (fhdr.num_ints != intdata) typematch = FALSE;
    fread(&intdata,4,1,fd);                             /* number of shorts */
    if (hdr->swapped) intdata = SWAPINT(intdata);
    if (fhdr.num_shorts != intdata) typematch = FALSE;
    fread(&intdata,4,1,fd);                             /* number of chars */
    if (hdr->swapped) intdata = SWAPINT(intdata);
    if (fhdr.num_chars != intdata) typematch = FALSE;
    if ((hdr->file_type != ESPS_SD) && (typematch == FALSE))
    {
	fprintf(stderr,"ESPS hdr: got lost in the header (record description)\n");
	delete_esps_hdr(hdr);
	return misc_read_error;
    }
    /* other types ... */
    fseek(fd,9*2,SEEK_CUR);                             /* other types */
    fseek(fd,hdr->num_fields*2,SEEK_CUR);               /* zeros */
    /* Now we can read the field names */
    hdr->field_name = walloc(char *,hdr->num_fields);

    fread(&name_flag, 2, 1, fd);
    if (hdr->swapped) name_flag = SWAPSHORT(name_flag);

    for (i=0; i < hdr->num_fields; i++)
	hdr->field_name[i] = esps_get_field_name(fd,hdr,name_flag); /* field names */
    if (hdr->file_type == ESPS_SD)
    {   /* Only one field 'samples' */
	if (!streq(hdr->field_name[0],"samples"))
	{
	    fprintf(stderr,"ESPS hdr: guessed wrong about FEA_SD file (no 'samples' field)\n");
	    delete_esps_hdr(hdr);
	    return misc_read_error;
	}
    }

    /* Now fea, feature and value -- but how many are there ? */
    while (ftell(fd) < preamble.data_offset-4)
    {
	esps_fea r = read_esps_fea(fd,hdr);              /* feas */
	if (r == NULL) break;
/*	print_esps_fea(r); */
	r->next = hdr->fea;
	hdr->fea = r;
	if (r->type == 1) 
	    break; /* I think this (filename) is last FEA */
    }
    /* There's other gunk after this but I think I've done enough */
    /* The rest seems to be mostly previous headers               */

    fseek(fd,hdr->hdr_size,SEEK_SET); /* skip the rest of the header */
    *uhdr = hdr;
	
    return format_ok;
}


enum EST_write_status write_esps_hdr(esps_hdr hdr,FILE *fd)
{
    /* well here's the scary part, try to write a valid file hdr to */
    /* the file                                                     */
    struct ESPS_PREAMBLE preamble;
    struct ESPS_FIXED_HDR fhdr;
    time_t tx = time(0);
    esps_fea t;
    int i,intdata;
    short shortdata;

    memset(&preamble,0,sizeof(preamble));
    memset(&fhdr,0,sizeof(fhdr));
    /* I can't really make the machine code work properly, so I'll  */
    /* just fix it for the two major byte orders to Sun and Suni386 */
    if (EST_NATIVE_BO == bo_big)
	preamble.machine_code = 4;  /* a sun */
    else
	preamble.machine_code = 6;  /* a suni386 */
    preamble.check_code  = 3000;  /* ? */
    preamble.data_offset = 0;  /* will come back and fix this later */
    preamble.record_size = esps_record_size(hdr);
    preamble.check = ESPS_MAGIC;
    preamble.edr = 0;
    preamble.fil1 = 0;
    preamble.foreign_hd = 0; /* docs say it should be -1, but its always 0 */

    fhdr.thirteen = 13;      /* must be for luck */
    fhdr.sdr_size = 0;
    fhdr.magic = ESPS_MAGIC;
    strncpy(fhdr.date,ctime(&tx),26);
    sprintf(fhdr.version,"1.91");  /* that's what all the others have */
    sprintf(fhdr.prog,"EDST");
    sprintf(fhdr.vers,"0.1");
    strncpy(fhdr.progcompdate,ctime(&tx),26);
    fhdr.num_samples = hdr->num_records;
    fhdr.filler = 0;
    /* in each record */
    fhdr.num_doubles = esps_num_of_type(ESPS_DOUBLE,hdr);
    fhdr.num_floats  = esps_num_of_type(ESPS_FLOAT,hdr);
    fhdr.num_ints    = esps_num_of_type(ESPS_INT,hdr);
    fhdr.num_shorts  = esps_num_of_type(ESPS_SHORT,hdr);
    fhdr.num_chars   = esps_num_of_type(ESPS_CHAR,hdr);
    fhdr.fsize = 40;
    fhdr.hsize = 0;    /* given value below on second shot */
    if (hdr->file_type == ESPS_SD)
	fhdr.fea_type = 8;
    else
	fhdr.fea_type = 0;
    fhdr.num_fields = hdr->num_fields;

    fwrite(&preamble,sizeof(preamble),1,fd);
    fwrite(&fhdr,sizeof(fhdr),1,fd);
    /* The following cover dimensions, type and ordering info */
    for (i=0; i < hdr->num_fields; i++)
    {   /* Dimensions (i.e. number of channels) */
	intdata = 1;
	fwrite(&intdata,4,1,fd);                        /* dimensions */
    }
    for (i=0; i < hdr->num_fields; i++)                 /* ordering info (?) */
	fwrite(&i,4,1,fd);
    if (hdr->file_type == ESPS_SD)  /* zeros hmm should be zeroes only */
	shortdata = 1;              /* is FEA case, 1 in ESPS_SD case  */
    else                            /* fixed 24/7/98                   */
	shortdata = 0;
    for (i=0; i < hdr->num_fields; i++)
	fwrite(&shortdata,2,1,fd);     
    for (i=0; i < hdr->num_fields; i++)    
    {
	shortdata = hdr->field_type[0];                 /* field types */
	fwrite(&shortdata,2,1,fd);
    }
    intdata = fhdr.num_doubles;                        /* number of doubles */
    fwrite(&intdata,4,1,fd);
    intdata = fhdr.num_floats;                         /* number of floats */
    fwrite(&intdata,4,1,fd);
    intdata = fhdr.num_ints;                           /* number of ints */
    fwrite(&intdata,4,1,fd);
    intdata = fhdr.num_shorts;                         /* number of shorts */
    fwrite(&intdata,4,1,fd);
    intdata = fhdr.num_chars;                          /* number of chars */
    fwrite(&intdata,4,1,fd);
    shortdata = 0;
    for (i=0; i < 9; i++)
	fwrite(&shortdata,2,1,fd);                      /* other types */
    for (i=0; i < hdr->num_fields; i++)
	fwrite(&shortdata,2,1,fd);                      /* zeros */
    /* Now dump the filednames */
    for (i=0; i < hdr->num_fields; i++)
	esps_put_field_name(hdr->field_name[i],fd,hdr); /* field names */
    if (hdr->file_type != ESPS_SD)
	fwrite(&shortdata,2,1,fd);   /* another 0 */
    /* Now the feas */
    for (t=hdr->fea; t != NULL; t=t->next)
	write_esps_fea(fd,t,hdr);                       /* feas */
    /* now have to go back and fix the header size */
    intdata = 0;
    fwrite(&intdata,4,1,fd);
    preamble.data_offset = ftell(fd);
    fhdr.hsize = (preamble.data_offset-249)/2;
    if (fseek(fd,0,SEEK_SET) == -1)
    {
	fprintf(stderr,"esps write header: can't fseek to start of file\n");
	return misc_write_error;
    }
    fwrite(&preamble,sizeof(preamble),1,fd);
    fwrite(&fhdr,sizeof(fhdr),1,fd);
    fseek(fd,preamble.data_offset,SEEK_SET);
    
    return write_ok;
}
