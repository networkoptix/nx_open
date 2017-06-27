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
/*                    Author :  Paul Taylor and Alan W Black             */
/*                    Date   :  June 1994 (June 1996)                    */
/*-----------------------------------------------------------------------*/
/*                   Access ESPS headered files                          */
/*  This offers a licence free interface to reading and writing ESPS     */  
/*  headered files. although not complete it works for most major ESPS   */
/*  files include signals, lpc and F0 files                              */
/*                                                                       */
/*=======================================================================*/
#include <cstdio>
#include <cstdlib>
#include "EST_unix.h"
#include <cstring>
#include "EST_cutils.h"
#include "EST_wave_utils.h"
#include "esps_utils.h"

/* The following are attempts to read and write ESPS header files without */
/* Using the Entropic libraries. These routines do not read all ESPS      */
/* headers but are adequate for all of the uses we wish (and those you    */
/* wish too probably)                                                     */

enum EST_write_status put_esps(const char *filename,const char *style, float *t, float *a, 
	     int *v, float fsize, float rate, int num_points)
{
    (void)t;
    esps_hdr hdr;
    esps_rec rec;
    FILE *fd;
    int i;

    if ((fd=fopen(filename,"wb")) == NULL)
    {
	fprintf(stderr,"ESPS file: cannot open file \"%s\" for writing\n",
		filename);
	return misc_write_error;
    }
	
    hdr = make_esps_hdr();
    
    if (streq(style,"F0"))
    {
	add_field(hdr,"F0",ESPS_DOUBLE,1);
	add_field(hdr,"prob_voice",ESPS_DOUBLE,1);
	add_field(hdr,"rms",ESPS_DOUBLE,1);
	add_field(hdr,"ac_peak",ESPS_DOUBLE,1);
	add_field(hdr,"k1",ESPS_DOUBLE,1);
	add_fea_d(hdr,"record_freq",0,(double)rate);
	add_fea_d(hdr,"frame_duration",0,(double)fsize);
	add_fea_d(hdr,"start_time",0,(double)0);
	add_fea_special(hdr,ESPS_FEA_COMMAND,
			"EDST F0 written as ESPS FEA_SD.\n");
	write_esps_hdr(hdr,fd);
	rec = new_esps_rec(hdr);
	for (i = 0; i < num_points; i++)
	{
	    set_field_d(rec,0,0,a[i]);
	    set_field_d(rec,1,0,(float)v[i]);
	    set_field_d(rec,2,0,0.5);
	    set_field_d(rec,3,0,0.5);
	    set_field_d(rec,4,0,0.5);
	    write_esps_rec(rec,hdr,fd);
	}
	delete_esps_rec(rec);
    }
    else
    {
	add_field(hdr,"Track",ESPS_DOUBLE,1);
	add_fea_d(hdr,"window_duration",0,(double)0.049);
	add_fea_d(hdr,"frame_duration",0,(double)fsize);
	add_fea_d(hdr,"record_freq",0,(double)rate);
	add_fea_d(hdr,"start_time",0,(double)0);
	add_fea_special(hdr,ESPS_FEA_COMMAND,
			"EDST Track written as ESPS FEA_SD.\n");
	write_esps_hdr(hdr,fd);
	rec = new_esps_rec(hdr);
	for (i = 0; i < num_points; i++)
	{
	    set_field_d(rec,0,0,a[i]);
	    write_esps_rec(rec,hdr,fd);
	}
	delete_esps_rec(rec);
    }

    delete_esps_hdr(hdr);
    fclose(fd);

    return write_ok;
}

enum EST_write_status put_track_esps(const char *filename, char **f_names, 
				     float **a, float fsize, float rate, 
				     int order, int num_points, short fixed)
{
    esps_hdr hdr;
    esps_rec rec;
    FILE *fd;
    int i, j;

    hdr = make_esps_hdr();

    if ((fd = fopen(filename, "wb")) == NULL)
    {
	fprintf(stderr,"ESPS file: cannot open file \"%s\" for writing\n",
		filename);
	return misc_write_error;
    }

    for (i = 0; i < order; ++i)
	add_field(hdr,f_names[i],ESPS_DOUBLE,1);

    if (!streq(f_names[0],"F0"))
    {
	add_fea_s(hdr,"lpccep_order",0,(short)order);
	add_fea_i(hdr,"step",0,(int)fsize); 
	add_fea_d(hdr,"window_duration",0,(double)0.049);
	add_fea_i(hdr,"start",0,(int)1);
	add_fea_f(hdr,"warping_param",0,(float)0.0);
	add_fea_s(hdr,"window_type",0,(short)2);
    }
    add_fea_d(hdr,"record_freq",0,(double)rate);
    add_fea_d(hdr,"frame_duration",0,(double)fsize);
    add_fea_d(hdr,"start_time",0,(double)0.0);
    if (!fixed)
      add_fea_s(hdr,"est_variable_frame", 0, (short)1);
    
    write_esps_hdr(hdr,fd);

    rec = new_esps_rec(hdr);
    for (i = 0; i < num_points; ++i)
    {
	for (j = 0; j < order; ++j)
	    set_field_d(rec, j, 0,(double)a[i][j]);
	write_esps_rec(rec,hdr,fd);
    }

    delete_esps_hdr(hdr);
    fclose(fd);
    return write_ok;
}

enum EST_read_status get_esps(const char *filename, char *style, 
	     float **t, float **a, int **v, float *fsize, int *num_points)
{
    (void)t;
    FILE *fd;
    enum EST_read_status rv;
    int ff0, fprob_voice, i;
    esps_hdr hdr;
    float *ta;
    double d;
    int *tv;
    esps_rec rec;

    if ((fd = fopen(filename, "rb")) == NULL)
    {
	fprintf(stderr, "Can't open esps file %s for reading\n", filename);
	return misc_read_error;
    }

    if ((rv=read_esps_hdr(&hdr,fd)) != format_ok)
    {
	fclose(fd);
	return rv;
    }
    ta = walloc(float,hdr->num_records);
    tv = walloc(int,hdr->num_records);
    /* Find field number of FO and prob_voice */
    for (ff0=fprob_voice=-1,i=0; i < hdr->num_fields; i++)
	if (streq("F0",hdr->field_name[i]))
	    ff0=i;
	else if (streq("prob_voice",hdr->field_name[i]))
	    fprob_voice = i;

    rec = new_esps_rec(hdr);
    for (i=0; i < hdr->num_records; i++)
    {
	if (read_esps_rec(rec,hdr,fd) == EOF)
	{
	    fprintf(stderr,"ESPS file: unexpected end of file when reading record %d\n", i);
	    delete_esps_rec(rec);
	    delete_esps_hdr(hdr);
	    fclose(fd);
	    return misc_read_error;
	}
	if (ff0 == -1)     /* F0 field isn't explicitly labelled */
	{                  /* so take first field                */
	    switch(rec->field[0]->type)
	    {
	      case ESPS_DOUBLE:
		ta[i] = get_field_d(rec,0,0); break;
	      case ESPS_FLOAT:
		ta[i] = get_field_f(rec,0,0); break;
	      default:
		fprintf(stderr,"ESPS file: doesn't seem to be F0 file\n");
		delete_esps_rec(rec);
		delete_esps_hdr(hdr);
		fclose(fd);
		return misc_read_error;
	    }
	}
	else    /* use named field -- assume its a double */
	    ta[i] = get_field_d(rec,ff0,0);
	if (fprob_voice == -1)
	    tv[i] = 1;   /* no prob_voice field in this */
	else
	    tv[i] = ((get_field_d(rec,fprob_voice,0) < 0.5) ? 0 : 1);
    }

    *num_points = hdr->num_records;
    *a = ta;
    *v = tv;
    if (fea_value_d("record_freq",0,hdr,&d) != 0)
	*fsize = 0;
    else
	*fsize = 1.0/d;
    if (ff0 != -1)
	strcpy(style, "F0");
    else 
	strcpy(style, "track");
    delete_esps_rec(rec);
    delete_esps_hdr(hdr);
    fclose(fd);

    return format_ok;
}

enum EST_read_status get_track_esps(const char *filename, char ***fields,
				    float ***a, float *fsize, 
				    int *num_points, int *num_fields,
				    short *fixed)
{
    esps_hdr hdr;
    esps_rec rec;
    FILE *fd;
    int i, j, order, num_recs;
    enum EST_read_status rv;
    double d;
    char **tf;
    float **ta;

    if ((fd = fopen(filename, "rb")) == NULL)
	return misc_read_error;

    if ((rv=read_esps_hdr(&hdr,fd)) != format_ok)
    {
	fclose(fd);
	return rv;
    }
    num_recs =  hdr->num_records;
    order =  hdr->num_fields;

    ta = walloc(float *,num_recs);
    tf = walloc(char *,order);
    for (j = 0; j < num_recs; ++j)
	ta[j] = walloc(float,order);

    /* Read data values */
    rec = new_esps_rec(hdr);

    {
      short v;
      *fixed = fea_value_s("est_variable_frame", 0, hdr, &v) != 0;
    }

    for (j = 0; j < hdr->num_records; j++)
    {
	if (read_esps_rec(rec,hdr,fd) == EOF)
	{
	    fprintf(stderr,"ESPS file: unexpected end of file when reading record %d\n", j);
	    delete_esps_rec(rec);
	    delete_esps_hdr(hdr);
	}
	for (i = 0; i < order; ++i)
	    switch (rec->field[i]->type)
	    {
	      case ESPS_DOUBLE:
		ta[j][i]=get_field_d(rec,i,0); break;
	      case ESPS_FLOAT:
		ta[j][i]=get_field_f(rec,i,0); break;
	      case ESPS_INT:
		ta[j][i]=(float)get_field_i(rec,i,0); break;
	      case ESPS_SHORT:
		ta[j][i]=(float)get_field_s(rec,i,0); break;
	      case ESPS_CHAR:
		ta[j][i]=(float)get_field_c(rec,i,0); break;
	      case ESPS_CODED:
		ta[j][i]=(float)get_field_s(rec,i,0); break;
	      default:
		fprintf(stderr,"ESPS file: unsupported type in record %d\n",
			rec->field[i]->type);
		delete_esps_rec(rec);
		delete_esps_hdr(hdr);
		fclose(fd);
		return misc_read_error;
	    }
    }
    num_recs = j; /* just a safe guard */

    /* read field names */
    for (j = 0; j < order; ++j)
	tf[j] = wstrdup(hdr->field_name[j]);

    /* copy local variables into argument list */
    *fields = tf;
    *num_points = num_recs;
    *num_fields = order;
    *a = ta;

    if (fea_value_d("record_freq",0,hdr,&d) != 0)
	*fsize = 0;
    else
	*fsize = 1.0/d;

    delete_esps_rec(rec);
    delete_esps_hdr(hdr);

    fclose(fd);
    return format_ok;
}

