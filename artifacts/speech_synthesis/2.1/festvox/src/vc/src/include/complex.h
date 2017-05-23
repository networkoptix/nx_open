/*********************************************************************/
/*                                                                   */
/*            Nagoya Institute of Technology, Aichi, Japan,          */
/*       Nara Institute of Science and Technology, Nara, Japan       */
/*                                and                                */
/*             Carnegie Mellon University, Pittsburgh, PA            */
/*                      Copyright (c) 2003-2004                      */
/*                        All Rights Reserved.                       */
/*                                                                   */
/*  Permission is hereby granted, free of charge, to use and         */
/*  distribute this software and its documentation without           */
/*  restriction, including without limitation the rights to use,     */
/*  copy, modify, merge, publish, distribute, sublicense, and/or     */
/*  sell copies of this work, and to permit persons to whom this     */
/*  work is furnished to do so, subject to the following conditions: */
/*                                                                   */
/*    1. The code must retain the above copyright notice, this list  */
/*       of conditions and the following disclaimer.                 */
/*    2. Any modifications must be clearly marked as such.           */
/*    3. Original authors' names are not deleted.                    */
/*                                                                   */    
/*  NAGOYA INSTITUTE OF TECHNOLOGY, NARA INSTITUTE OF SCIENCE AND    */
/*  TECHNOLOGY, CARNEGIE MELLON UNIVERSITY, AND THE CONTRIBUTORS TO  */
/*  THIS WORK DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  */
/*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, */
/*  IN NO EVENT SHALL NAGOYA INSTITUTE OF TECHNOLOGY, NARA           */
/*  INSTITUTE OF SCIENCE AND TECHNOLOGY, CARNEGIE MELLON UNIVERSITY, */
/*  NOR THE CONTRIBUTORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR      */
/*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM   */
/*  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  */
/*  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        */
/*  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.         */
/*                                                                   */
/*********************************************************************/
/*                                                                   */
/*          Author :  Hideki Banno                                   */
/*                                                                   */
/*-------------------------------------------------------------------*/
/*                                                                   */
/*  Slightly modified by Tomoki Toda (tomoki@ics.nitech.ac.jp)       */
/*  June 2004                                                        */
/*  Integrate as a Voice Conversion module                           */
/*                                                                   */
/*-------------------------------------------------------------------*/

#ifndef __COMPLEX_H
#define __COMPLEX_H

#include "vector.h"

typedef struct FCOMPLEX_STRUCT {
    long length;
    FVECTOR real;
    FVECTOR imag;
} *FCOMPLEX;

typedef struct DCOMPLEX_STRUCT {
    long length;
    DVECTOR real;
    DVECTOR imag;
} *DCOMPLEX;

extern FCOMPLEX xfcalloc(long length);
extern FCOMPLEX xfczeros(long length);
extern void xfcfree(FCOMPLEX cplx);
extern void fccopy(FCOMPLEX cplx, FVECTOR real, FVECTOR imag);
extern FCOMPLEX xfccreate(FVECTOR real, FVECTOR imag, long length);

extern DCOMPLEX xdcalloc(long length);
extern DCOMPLEX xdczeros(long length);
extern void xdcfree(DCOMPLEX cplx);
extern void dccopy(DCOMPLEX cplx, DVECTOR real, DVECTOR imag);
extern DCOMPLEX xdccreate(DVECTOR real, DVECTOR imag, long length);

extern DVECTOR xdcpower(DCOMPLEX cplx);
extern DVECTOR xdcabs(DCOMPLEX cplx);
extern DVECTOR xdvcabs(DVECTOR real, DVECTOR imag, long length);
extern DVECTOR xdvcpower(DVECTOR real, DVECTOR imag, long length);

extern DCOMPLEX xdvcexp(DVECTOR real, DVECTOR imag, long length);
extern void dcexp(DCOMPLEX cplx);
extern DCOMPLEX xdcexp(DCOMPLEX x);

#define xfcreal(cplx) xfvclone((cplx)->real)
#define xfcimag(cplx) xfvclone((cplx)->imag)
#define xdcreal(cplx) xdvclone((cplx)->real)
#define xdcimag(cplx) xdvclone((cplx)->imag)

#define xfcclone(cplx) xfccreate((cplx)->real, (cplx)->imag, (cplx)->length)
#define xdcclone(cplx) xdccreate((cplx)->real, (cplx)->imag, (cplx)->length)

#endif /* __COMPLEX_H */
