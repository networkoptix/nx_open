/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
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
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                                                                       */
/*                   Author :  Korin Richmond                            */
/*                     Date :  28 August 2003                            */
/* -------------------------------------------------------------------   */
/* SWIG interface file for EST_TSimpleVector template class              */
/* (Primarily meant to be included in other interface files where        */
/* instantiation and/or inheritance take place)                          */
/*                                                                       */
/*************************************************************************/

%{
#include "EST_TSimpleVector.h"
%}

%include "EST_TVector.i"

template <class T> 
class EST_TSimpleVector : public EST_TVector<T> {
public:
    EST_TSimpleVector() : EST_TVector<T>() {}; 
    EST_TSimpleVector(const EST_TSimpleVector<T> &v);
    // "size" constructor
    EST_TSimpleVector(int n): EST_TVector<T>(n) {}; 

    // resize vector
    void resize(int n, int set=1); 

    // assignment operator
    //EST_TSimpleVector &operator=(const EST_TSimpleVector<T> &s); 

    void copy_section(T* dest, int offset=0, int num=-1) const;
    void set_section(const T* src, int offset=0, int num=-1);

    // Fill entire array with 0 bits. 
    void zero(void);

    // Fill vector with default value (might be 0)
    void empty(void);
};
