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
/* SWIG interface file for EST_TVector template class                    */
/* (Primarily meant to be included in other interface files where        */
/* instantiation and/or inheritance take place)                          */
/*                                                                       */
/*************************************************************************/

%{
#include "EST_TVector.h"
%}

template <class T> 
class EST_TVector 
{
public:
  EST_TVector(); 
  EST_TVector(const EST_TVector<T> &v); 

  // "size" constructor - make vector of size n.
  EST_TVector(int n); 

  // construct from memory supplied by caller
  EST_TVector(int, T *memory, int offset=0, int free_when_destroyed=0);

  ~EST_TVector();

  void resize(int n, int set=1); 

  // number of items in vector.
  int num_columns() const;
  int length() const;
  int n() const;

  // access without bounds checking
  const T &a_no_check(int n);
  const T &a_no_check_1(int n);

  // access with bounds checking
  const T &a_check(int n);
  const T &a(int n);

  // assignment operator
  //EST_TVector &operator=(const EST_TVector &s);

  // Fill with value v
  void fill(const T &v);

  // Fill with default value
  void empty();

  // true if equal size and all elements are equal.
  int operator == (const EST_TVector &v) const;
  // true if not equal size or a single elements isn't equal.
  int operator != (const EST_TVector &v) const;


#if defined(SWIGPYTHON)
  %rename (__setitem__) setitem;
  %rename (__getitem__) getitem;
#endif //SWIGPYTHON

  %extend {
    void setitem( int i, const T& val ){
      self->a_check(i) = val;
    }
    
    const T& getitem( int i ){
      return self->a_check(i);
    }
  }

  // Copy data in and out. Subclassed by SimpleVector for speed.
  void copy_section(T* dest, int offset=0, int num=-1) const;
  void set_section(const T* src, int offset=0, int num=-1);
};

template<class T>
extern ostream& operator << (ostream &st, const EST_TVector< T > &a);
