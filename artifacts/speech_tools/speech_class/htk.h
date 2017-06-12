 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
 /*                        All Rights Reserved.                          */
 /*                                                                      */
 /*  Permission is hereby granted, free of charge, to use and distribute */
 /*  this software and its documentation without restriction, including  */
 /*  without limitation the rights to use, copy, modify, merge, publish, */
 /*  distribute, sublicense, and/or sell copies of this work, and to     */
 /*  permit persons to whom this work is furnished to do so, subject to  */
 /*  the following conditions:                                           */
 /*   1. The code must retain the above copyright notice, this list of   */
 /*      conditions and the following disclaimer.                        */
 /*   2. Any modifications must be clearly marked as such.               */
 /*   3. Original authors' names are not deleted.                        */
 /*   4. The authors' names are not used to endorse or promote products  */
 /*      derived from this software without specific prior written       */
 /*      permission.                                                     */
 /*                                                                      */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
 /*  THIS SOFTWARE.                                                      */
 /*                                                                      */
 /*************************************************************************/
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /*                   Date: Mon Nov  3 1997                               */
 /* --------------------------------------------------------------------  */
 /* Defines for HTK format.                                               */
 /*                                                                       */
 /*************************************************************************/



#ifndef __HTK_H__
#define __HTK_H__

#define HTK_ENERGY   0x0040
#define HTK_NO_E     0x0080
#define HTK_DELTA    0x0100
#define HTK_AC       0x0200
#define HTK_COMP     0x0400
#define HTK_ZM       0x0800
#define HTK_CRC      0x1000
#define HTK_0CEP     0x2000
#define HTK_EST_PS   0x4000	// This is a local one and may become illegal
#define HTK_MASK     0x003f

#define HTK_WAVE     0x0000
#define HTK_LPC      0x0001
#define HTK_LPCREFC  0x0002
#define HTK_LPCCEP   0x0003
#define HTK_LPDELCEP 0x0004
#define HTK_IREFC    0x0005
#define HTK_MFCC     0x0006
#define HTK_FBANK    0x0007
#define HTK_MELSPEC  0x0008
#define HTK_USER     0x0009
#define HTK_DISCRETE 0x000A


/* 100ns = 10 per us = 10 000 per ms = 10 000 000 per second*/
#define HTK_UNITS_PER_SECOND 10000000

struct htk_header 
{
    int num_samps; 
    int samp_period;   /* in 100ns units -design choice of the century*/
    /* I don't think this is right -- awb, is this an old HTK format ? */
    short samp_size;    /* number of bytes per sample */
    short samp_type;    /* code from 0 to 10 indicating sample kind */
                        /* - see HTK reference manual, or #defs above */
};


#endif
