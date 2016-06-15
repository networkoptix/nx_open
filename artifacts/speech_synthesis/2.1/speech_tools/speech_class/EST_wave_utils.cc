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
/*                  Author :  Alan Black and Paul Taylor                 */
/*                  Date   :  June 1996                                  */
/*-----------------------------------------------------------------------*/
/*  Various low-level waveform conversion routines and file format       */
/*  independent i/o functions                                            */
/*                                                                       */
/* Acknowledgements                                                      */
/* ulaw conversion code provided by                                      */
/*     Craig Reese: IDA/Supercomputing Research Center                   */
/* IEEE extended conversion                                              */
/*     Apple Computer, Inc.                                               */
/*                                                                       */
/*=======================================================================*/
#include <cstdio>
#include <cstdlib> 
#include "EST_unix.h"
#include <cstring>
#include <cmath>
#include "EST_wave_utils.h"
#include "EST_wave_aux.h"
#include "EST_error.h"

static short st_ulaw_to_short(unsigned char ulawbyte);
static unsigned char st_short_to_ulaw(short sample);

/*
 * This table is 
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 * take from speak_freely-7.2/gsm/src/toast_alaw.c
 */
static unsigned short a2s[] = {
         5120,60160,  320,65200,20480,44032, 1280,64192,
         2560,62848,   64,65456,10240,54784,  640,64864,
         7168,58112,  448,65072,28672,35840, 1792,63680,
         3584,61824,  192,65328,14336,50688,  896,64608,
         4096,61184,  256,65264,16384,48128, 1024,64448,
         2048,63360,    0,65520, 8192,56832,  512,64992,
         6144,59136,  384,65136,24576,39936, 1536,63936,
         3072,62336,  128,65392,12288,52736,  768,64736,
         5632,59648,  352,65168,22528,41984, 1408,64064,
         2816,62592,   96,65424,11264,53760,  704,64800,
         7680,57600,  480,65040,30720,33792, 1920,63552,
         3840,61568,  224,65296,15360,49664,  960,64544,
         4608,60672,  288,65232,18432,46080, 1152,64320,
         2304,63104,   32,65488, 9216,55808,  576,64928,
         6656,58624,  416,65104,26624,37888, 1664,63808,
         3328,62080,  160,65360,13312,51712,  832,64672,
         5376,59904,  336,65184,21504,43008, 1344,64128,
         2688,62720,   80,65440,10752,54272,  672,64832,
         7424,57856,  464,65056,29696,34816, 1856,63616,
         3712,61696,  208,65312,14848,50176,  928,64576,
         4352,60928,  272,65248,17408,47104, 1088,64384,
         2176,63232,   16,65504, 8704,56320,  544,64960,
         6400,58880,  400,65120,25600,38912, 1600,63872,
         3200,62208,  144,65376,12800,52224,  800,64704,
         5888,59392,  368,65152,23552,40960, 1472,64000,
         2944,62464,  112,65408,11776,53248,  736,64768,
         7936,57344,  496,65024,31744,32768, 1984,63488,
         3968,61440,  240,65280,15872,49152,  992,64512,
         4864,60416,  304,65216,19456,45056, 1216,64256,
         2432,62976,   48,65472, 9728,55296,  608,64896,
         6912,58368,  432,65088,27648,36864, 1728,63744,
         3456,61952,  176,65344,13824,51200,  864,64640
};

#define st_alaw_to_short(a) (a2s[(unsigned char)a])

void ulaw_to_short(const unsigned char *ulaw,short *data,int length)
{
    /* Convert ulaw to shorts */
    int i;

    for (i=0; i<length; i++)
	data[i] = st_ulaw_to_short(ulaw[i]);  /* ulaw convert */
    
}

void alaw_to_short(const unsigned char *alaw,short *data,int length)
{
    /* Convert ulaw to shorts */
    int i;

    for (i=0; i<length; i++)
    {
	data[i] = st_alaw_to_short(alaw[i])-32768;  /* alaw convert */
    }
}

void shorten_to_short(unsigned char *ulaw, short *data, int length)
{
    /* Convert Tony Robinson's shorten format to shorts */
    (void)ulaw;
    (void)data;
    (void)length;
    
}

void uchar_to_short(const unsigned char *chars,short *data,int length)
{
    /* Convert 8 bit data to shorts  UNSIGNED CHAR */
    int i;

    for (i=0; i<length; i++)
      {
      data[i] = (((int)chars[i])-128)*256;
      }
    
}

void schar_to_short(const unsigned char *chars,short *data,int length)
{
    /* Convert 8 bit data to shorts SIGNED CHAR */
    int i;

    for (i=0; i<length; i++)
	data[i] = (((unsigned char)chars[i]))*256;
    
}

void short_to_uchar(const short *data,unsigned char *chars,int length)
{
    /* Convert shorts to 8 bit UNSIGNED CHAR */
    int i;

    for (i=0; i<length; i++)
	chars[i] = (data[i]/256)+128;
    
}

void short_to_schar(const short *data,unsigned char *chars,int length)
{
    /* Convert shorts to 8 bit SIGNED CHAR */
    int i;

    for (i=0; i<length; i++)
	chars[i] = (data[i]/256);
    
}

#if 0
void short_to_adpcm(short *data,signed char *chars,int length)
{
    struct adpcm_state state;
    state.valprev = 0;
    state.index = 0;

    adpcm_coder(data,chars,length,&state);

}

void adpcm_to_short(signed char *chars, short *data,int length)
{
    struct adpcm_state state;
    state.valprev = 0;
    state.index = 0;

    adpcm_decoder(chars,data,length/2,&state);
}
#endif

void short_to_ulaw(const short *data,unsigned char *ulaw,int length)
{
    /* Convert ulaw to shorts */
    int i;

    for (i=0; i<length; i++)
	ulaw[i] = st_short_to_ulaw(data[i]);  /* ulaw convert */
    
}

short *convert_raw_data(unsigned char *file_data,int data_length,
			enum EST_sample_type_t sample_type,int bo)
{
    /* converts data in file_data to native byte order shorts */
    /* frees file_data if its not returned                    */
    short *d=NULL;

    if (sample_type == st_short)
    {
      // d = walloc(short,data_length);
	if (bo != EST_NATIVE_BO)
	    swap_bytes_short((short *)file_data,data_length);
	return (short *)file_data;
    }
    else if (sample_type == st_mulaw)
    {
	d = walloc(short,data_length);
	ulaw_to_short(file_data,d,data_length);
	wfree(file_data);
	return d;
    }
    else if (sample_type == st_alaw)
    {
	d = walloc(short,data_length);
	alaw_to_short(file_data,d,data_length);
	wfree(file_data);
	return d;
    }
#if 0
    else if (sample_type == st_adpcm)
    {   /* Not really checked yet */
	d = walloc(short,data_length);
	adpcm_to_short((signed char *)file_data,d,data_length);
	wfree(file_data);
	return d;
    }
#endif
    else if (sample_type == st_schar)
    {
	d = walloc(short,data_length);
	schar_to_short((unsigned char *)file_data,d,data_length);
	wfree(file_data);
	return d;
    }
    else if (sample_type == st_uchar)
    {
	d = walloc(short,data_length);
	uchar_to_short((unsigned char *)file_data,d,data_length);
	wfree(file_data);
	return d;
    }
    else
      EST_error("Convert raw data: unsupported sample type %s(%d)",
		EST_sample_type_map.name(sample_type), sample_type);

    /* NOTREACHED */
    return NULL;
}

enum EST_write_status save_raw_data(FILE *fp, const short *data, int offset,
				    int num_samples, int num_channels, 
				    enum EST_sample_type_t sample_type, 
				    int bo)
{
    int i;
    int n;

    if (sample_type == st_mulaw)
    {
	unsigned char *ulaw = walloc(unsigned char,num_samples*num_channels);
	short_to_ulaw(data+(offset*num_channels),
		      ulaw,num_samples*num_channels);
	n = fwrite(ulaw,1,num_channels * num_samples,fp);	
	wfree(ulaw);
	if (n != (num_channels * num_samples))
	    return misc_write_error;
    }
    else if (sample_type == st_ascii)
    {
	for (i=offset*num_channels; i < num_samples*num_channels; i++)
	    fprintf(fp,"%d\n",data[i]);
    }
    else if (sample_type == st_schar)
    {
	unsigned char *chars = walloc(unsigned char,num_samples*num_channels);
	short_to_schar(data+(offset*num_channels),
		       chars,num_samples*num_channels);
	n = fwrite(chars,1,num_channels * num_samples,fp);
	wfree(chars);
	if (n != (num_channels * num_samples))
	    return misc_write_error;
    }
    else if (sample_type == st_uchar)
    {
	unsigned char *chars = walloc(unsigned char,num_samples*num_channels);
	short_to_uchar(data+(offset*num_channels),
		       chars,num_samples*num_channels);
	n = fwrite(chars,1,num_channels * num_samples,fp);
	wfree(chars);
	if ( n != (num_channels * num_samples))
	    return misc_write_error;
    }
#if 0
    else if (sample_type == st_adpcm)
    {
	signed char *chars = walloc(signed char,num_samples*num_channels);
	short_to_adpcm(data+(offset*num_channels),
		       chars,num_samples*num_channels);
	n = fwrite(chars,1,num_channels * num_samples,fp);
	wfree(chars);
	if ( n != (num_channels * num_samples))
	    return misc_write_error;
    }
#endif
    else if (sample_type == st_short)
    {
	if (bo != EST_NATIVE_BO)
	{
	    short *xdata = walloc(short,num_channels*num_samples);
	    memmove(xdata,data+(offset*num_channels),
		    num_channels*num_samples*sizeof(short));
	    swap_bytes_short(xdata,num_channels*num_samples);
	    n = fwrite(xdata, sizeof(short),num_channels * num_samples, fp);
	    wfree(xdata);
	}
	else
	    n = fwrite(&data[offset], sizeof(short), 
		       num_channels * num_samples, fp);
	if (n != (num_channels * num_samples))
	    return misc_write_error;
    }
    else
    {
	fprintf(stderr,"save data file: unsupported sample type\n");
	return misc_write_error;
    }
    return write_ok;
}

int get_word_size(enum EST_sample_type_t sample_type)
{
    /* Returns word size from type */
    int word_size;

    switch (sample_type) 
    {
      case st_unknown:  
	word_size = 2;	break;
      case st_uchar:  
      case st_schar:  
	word_size = 1;	break;
      case st_mulaw:
	word_size = 1;	break;
#if 0
      case st_adpcm:  /* maybe I mean 0.5 */
	word_size = 1;	break;
#endif
      case st_short: 
	word_size = 2;	break;
      case st_int: 
	/* Yes I mean 4 not sizeof(int) these are machine independent defs */
	word_size = 4;	break;
      case st_float: 
	word_size = 4;	break;
      case st_double: 
	word_size = 8;	break;
      default:
	fprintf(stderr,"Unknown encoding format error\n");
	word_size = 2;
    }
    return word_size;
}

enum EST_sample_type_t str_to_sample_type(const char *type)
{
    /* string to internal value */

    if (streq(type,"short"))
	return st_short;
    if (streq(type,"shorten"))
	return st_shorten;
    else if ((streq(type,"ulaw")) || (streq(type,"mulaw")))
	return st_mulaw;
    else if ((streq(type,"char")) || (streq(type,"byte")) || 
	     (streq(type,"8bit")))
	return st_schar;
    else if ((streq(type,"unsignedchar")) || (streq(type,"unsignedbyte")) || 
	     (streq(type,"unsigned8bit")))
	return st_uchar;
    else if (streq(type,"int"))
	return st_int;
#if 0
    else if (streq(type,"adpcm"))
	return st_adpcm;
#endif
    else if ((streq(type,"real")) || (streq(type,"float")) || 
	     (streq(type,"real4")))
	return st_float;
    else if ((streq(type,"real8")) || (streq(type,"double")))
	return st_double;
    else if (streq(type,"alaw"))
	return st_alaw;
    else if (streq(type,"ascii"))
	return st_ascii;
    else
    {
	fprintf(stderr,"Unknown sample type: \"%s\"\n",type);
	return st_unknown;
    }
}

const char *sample_type_to_str(enum EST_sample_type_t type)
{
    switch (type)
    {
      case st_short:   return "short";
      case st_shorten:   return "shorten";
      case st_mulaw:   return "ulaw";
      case st_schar:    return "char";
      case st_uchar:    return "unsignedchar";
      case st_int:     return "int";
#if 0
      case st_adpcm:   return "adpcm";
#endif
      case st_float:   return "float";
      case st_double:  return "double";
      case st_ascii:   return "ascii";
      case st_unknown: return "unknown";
      default:
	fprintf(stderr,"Unknown sample_type %d\n",type);
	return "very_unknown";
    }
}

/*
** This routine converts from linear to ulaw.
**
** Craig Reese: IDA/Supercomputing Research Center
** Joe Campbell: Department of Defense
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) "A New Digital Technique for Implementation of Any
**     Continuous PCM Companding Law," Villeret, Michel,
**     et al. 1973 IEEE Int. Conf. on Communications, Vol 1,
**     1973, pg. 11.12-11.17
** 3) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: Signed 16 bit linear sample
** Output: 8 bit ulaw sample
*/

#define ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define CLIP 32635

static unsigned char st_short_to_ulaw(short sample)
{
    static int exp_lut[256] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
				   4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
				   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
				   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
				   6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
				   6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
				   6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
				   6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
				   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
    int sign, exponent, mantissa;
    unsigned char ulawbyte;

    /* Get the sample into sign-magnitude. */
    sign = (sample >> 8) & 0x80; /* set aside the sign */
    if ( sign != 0 ) sample = -sample; /* get magnitude */
    if ( sample > CLIP ) sample = CLIP; /* clip the magnitude */

    /* Convert from 16 bit linear to ulaw. */
    sample = sample + BIAS;
    exponent = exp_lut[( sample >> 7 ) & 0xFF];
    mantissa = ( sample >> ( exponent + 3 ) ) & 0x0F;
    ulawbyte = ~ ( sign | ( exponent << 4 ) | mantissa );
#ifdef ZEROTRAP
    if ( ulawbyte == 0 ) ulawbyte = 0x02; /* optional CCITT trap */
#endif

    return ulawbyte;
}

/*
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
*/

static short st_ulaw_to_short( unsigned char ulawbyte )
{
    static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
    int sign, exponent, mantissa;
    short sample;

    ulawbyte = ~ ulawbyte;
    sign = ( ulawbyte & 0x80 );
    exponent = ( ulawbyte >> 4 ) & 0x07;
    mantissa = ulawbyte & 0x0F;
    sample = exp_lut[exponent] + ( mantissa << ( exponent + 3 ) );
    if ( sign != 0 ) sample = -sample;

    return sample;
}

/*
 * C O N V E R T   T O   I E E E   E X T E N D E D
 */

/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define FloatToUnsigned(f)      ((unsigned long)(((long)(f - 2147483648.0)) + 2147483647L) + 1)

void ConvertToIeeeExtended(double num,unsigned char *bytes)
{
    int    sign;
    int expon;
    double fMant, fsMant;
    unsigned long hiMant, loMant;

    if (num < 0) {
        sign = 0x8000;
        num *= -1;
    } else {
        sign = 0;
    }

    if (num == 0) {
        expon = 0; hiMant = 0; loMant = 0;
    }
    else {
        fMant = frexp(num, &expon);
        if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
            expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
        }
        else {    /* Finite */
            expon += 16382;
            if (expon < 0) {    /* denormalized */
                fMant = ldexp(fMant, expon);
                expon = 0;
            }
            expon |= sign;
            fMant = ldexp(fMant, 32);          
            fsMant = floor(fMant); 
            hiMant = FloatToUnsigned(fsMant);
            fMant = ldexp(fMant - fsMant, 32); 
            fsMant = floor(fMant); 
            loMant = FloatToUnsigned(fsMant);
        }
    }
    
    bytes[0] = expon >> 8;
    bytes[1] = expon;
    bytes[2] = hiMant >> 24;
    bytes[3] = hiMant >> 16;
    bytes[4] = hiMant >> 8;
    bytes[5] = hiMant;
    bytes[6] = loMant >> 24;
    bytes[7] = loMant >> 16;
    bytes[8] = loMant >> 8;
    bytes[9] = loMant;
}


/*
 * C O N V E R T   F R O M   I E E E   E X T E N D E D  
 */

/* 
 * Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /*HUGE_VAL*/

# define UnsignedToFloat(u)         (((double)((long)(u - 2147483647L - 1))) + 2147483648.0)

/****************************************************************
 * Extended precision IEEE floating-point conversion routine.
 ****************************************************************/

double ConvertFromIeeeExtended(unsigned char *bytes)
{
    double    f;
    int    expon;
    unsigned long hiMant, loMant;
    
    expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
    hiMant    =    ((unsigned long)(bytes[2] & 0xFF) << 24)
            |    ((unsigned long)(bytes[3] & 0xFF) << 16)
            |    ((unsigned long)(bytes[4] & 0xFF) << 8)
            |    ((unsigned long)(bytes[5] & 0xFF));
    loMant    =    ((unsigned long)(bytes[6] & 0xFF) << 24)
            |    ((unsigned long)(bytes[7] & 0xFF) << 16)
            |    ((unsigned long)(bytes[8] & 0xFF) << 8)
            |    ((unsigned long)(bytes[9] & 0xFF));

    if (expon == 0 && hiMant == 0 && loMant == 0) {
        f = 0;
    }
    else {
        if (expon == 0x7FFF) {    /* Infinity or NaN */
            f = HUGE_VAL;
        }
        else {
            expon -= 16383;
            f  = ldexp((double)(hiMant), expon-=31);
            f += ldexp((double)(loMant), expon-=32);
        }
    }

    if (bytes[0] & 0x80)
        return -f;
    else
        return f;
}

