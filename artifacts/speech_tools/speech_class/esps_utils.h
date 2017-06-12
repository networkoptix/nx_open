/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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
/*                 Author :  Alan Black                                  */
/*                 Date   :  June 1996                                   */
/*-----------------------------------------------------------------------*/
/* Licence free version of esps file i/o functions: headers              */
/*                                                                       */
/*=======================================================================*/
#ifndef __ESPS_IO_H__
#define __ESPS_IO_H__

#define ESPS_MAGIC 27162
struct ESPS_PREAMBLE {
    int machine_code;   /* the machine that generated this (4 is sun) */
    int check_code;     /* dunno */
    int data_offset;    /* offset from start to start of data records */
    int record_size;    /* data record size in bytes */
    int check;          /* ESPS magic number */
    int edr;            /* byte order independent order or native */
    int fil1;           /* dunno */
    int foreign_hd;     /* foreign header -- not supported */
};
struct ESPS_FIXED_HDR {
    short thirteen;     /* seems to be always 13 */
    short sdr_size;     /* always 0 */
    int magic;          /* magic number again */
    char date[26];      /* file creation date */
    char version[8];    /* header version */
    char prog[16];      /* program used to create file */
    char vers[8];       /* program version */
    char progcompdate[26];  /* when that program was compile d */
    int num_samples;    /* number of samples (can be 0) */
    int filler;
    int num_doubles;    /* num of doubles in record */
    int num_floats;
    int num_ints;
    int num_shorts; 
    int num_chars;
    int fsize;          /* always 40 */
    int hsize;          /* wish I knew, it does vary */
    char username[8];   /* user who created this file */
    int fil1[5];        /* dunno */
    short fea_type;     /* may be */
    short fil2;
    short num_fields;   /* number of fields in a record */
    short fil3;
    int fil4[9];        /* a bunch of numbers that look like addresses */
    int fil5[8];        /* all zeros */
};

struct ESPS_FEA_struct {
    short type;
    short clength;
    char *name;
    int count;
    short dtype;
    union
    {
	int *ival;
	char *cval;
	float *fval;
	double *dval;
	short *sval;
    } v;
    struct ESPS_FEA_struct *next;
};
typedef struct ESPS_FEA_struct *esps_fea;

/* FEA files consist of record which can contain fields (off different */
/* data types) The following is used to represent arbitrary records    */
/* names of the records are given in the header structure              */
struct ESPS_FIELD_struct {
    int type;
    int dimension;
    union
    {
	int *ival;
	char *cval;
	float *fval;
	double *dval;
	short *sval;
    } v;
};
typedef struct ESPS_FIELD_struct *esps_field;

struct ESPS_REC_struct {
    int num_fields;
    int size;
    esps_field *field;
};
typedef struct ESPS_REC_struct *esps_rec;

enum esps_file_type {ESPS_FEA, ESPS_SD, ESPS_SPGRAM, ESPS_FILT};

/* This is what the user gets/gives, just the useful information */
struct ESPS_HDR_struct {
    enum esps_file_type file_type;
    int swapped;                     /* byte order in file */
    int hdr_size;          /* full size of file header in bytes */
    int num_records;
    int num_fields;
    char **field_name;
    short *field_type;
    int *field_dimension;
    esps_fea fea;                   /* list of FEA */
};
typedef struct ESPS_HDR_struct *esps_hdr;

#define ESPS_DOUBLE 1
#define ESPS_FLOAT  2
#define ESPS_INT    3
#define ESPS_SHORT  4
#define ESPS_CHAR   5   /* I doubt I'm treating char and byte appropriately */
#define ESPS_CODED  7   /* enumerated type. Same size as short */
#define ESPS_BYTE   8
/* There are others too including COMPLEX ones */

/* Some random numbers on FEA records */
#define ESPS_FEA_FILE      1
#define ESPS_FEA_DIRECTORY 15
#define ESPS_FEA_COMMAND   11

esps_fea new_esps_fea(void);
void delete_esps_fea(esps_fea r);
void print_esps_fea(esps_fea r);
esps_fea read_esps_fea(FILE *fd, esps_hdr hdr);
void write_esps_fea(FILE *fd, esps_fea t, esps_hdr hdr);
esps_hdr make_esps_hdr(void);
esps_hdr make_esps_sd_hdr(void);
void delete_esps_hdr(esps_hdr h);
enum EST_read_status read_esps_hdr(esps_hdr *hdr,FILE *fd);
enum EST_write_status write_esps_hdr(esps_hdr hdr,FILE *fd);

int fea_value_d(const char *name,int pos,esps_hdr hdr,double *d);
int fea_value_f(const char *name,int pos,esps_hdr hdr,float *d);
int fea_value_s(const char *name,int pos,esps_hdr hdr,short *d);
int fea_value_i(const char *name,int pos,esps_hdr hdr,int *d);
int fea_value_c(const char *name,int pos,esps_hdr hdr,char *d);

double get_field_d(esps_rec r, int field, int pos);
float get_field_f(esps_rec r, int field, int pos);
int get_field_i(esps_rec r, int field, int pos);
short get_field_s(esps_rec r, int field, int pos);
char get_field_c(esps_rec r, int field, int pos);
void set_field_d(esps_rec r, int field, int pos, double d);
void set_field_f(esps_rec r, int field, int pos, float d);
void set_field_i(esps_rec r, int field, int pos, int d);
void set_field_s(esps_rec r, int field, int pos, short d);
void set_field_c(esps_rec r, int field, int pos, char d);
esps_rec new_esps_rec(esps_hdr hdr);
void delete_esps_rec(esps_rec r);
int read_esps_rec(esps_rec r, esps_hdr h, FILE *fd);
int write_esps_rec(esps_rec r, esps_hdr h, FILE *fd);

void add_field(esps_hdr hdr,const char *name, int type, int dimension);
void add_fea_d(esps_hdr hdr,const char *name, int pos, double d);
void add_fea_s(esps_hdr hdr,const char *name, int pos, short d);
void add_fea_i(esps_hdr hdr,const char *name, int pos, int d);
void add_fea_f(esps_hdr hdr,const char *name, int pos, float d);
void add_fea_c(esps_hdr hdr,const char *name, int pos, char d);
void add_fea_special(esps_hdr hdr,int type,const char *name);

#endif /* __ESPS_IO_H__ */


