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
/*                  Author :  Alan Black                                 */
/*                  Date   :  June 1996                                  */
/*-----------------------------------------------------------------------*/
/*  General byte swapping routines                                       */
/*************************************************************************/

#include "EST_cutils.h"

/* For endian tests */
int est_endian_loc = 1;

void swapdouble(double *d)
{
    int *ii = (int *)d;
    int t;
    t = SWAPINT(ii[0]);
    ii[0] = SWAPINT(ii[1]);
    ii[1] = t;
}

void swapfloat(float *f)
{
    int *ii = (int *)f;
    ii[0] = SWAPINT(ii[0]);
}

void swap_bytes_float(float *data, int length)
{
    /* Swap floats in an array */
    int i;

    for (i=0; i<length; i++)
	swapfloat(&data[i]);
}

void swap_bytes_double(double *data, int length)
{
    /* Swap doubles in an array */
    int i;

    for (i=0; i<length; i++)
	swapdouble(&data[i]);
}

void swap_bytes_int(int *data, int length)
{
    /* Swap ints in an array */
    int i;

    for (i=0; i<length; i++)
	data[i] = SWAPINT(data[i]);

}

void swap_bytes_uint(unsigned int *data, int length)
{
    /* Swap ints in an array */
    int i;

    for (i=0; i<length; i++)
	data[i] = SWAPINT(data[i]);

}

void swap_bytes_short(short *data, int length)
{
    /* Swap shorts in an array */
    int i;

    for (i=0; i<length; i++)
	data[i] = SWAPSHORT(data[i]);

}

void swap_bytes_ushort(unsigned short *data, int length)
{
    /* Swap shorts in an array */
    int i;

    for (i=0; i<length; i++)
	data[i] = SWAPSHORT(data[i]);

}

