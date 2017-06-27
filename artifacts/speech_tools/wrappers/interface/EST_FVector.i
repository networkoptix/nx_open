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
/* SWIG interface file for EST_FVector class                             */
/*                                                                       */
/*************************************************************************/

%module EST_FVector

%{
#include "EST_FMatrix.h"
%}


%include "EST_rw_status.i"
%include "EST_typemaps.i"

%include "EST_TSimpleVector.i"
%template(floatvector) EST_TVector<float>;
%template(floatsimplevector) EST_TSimpleVector<float>;

class EST_FVector: public EST_TSimpleVector<float> {
public:
  EST_FVector(): EST_TSimpleVector<float>() {}
  EST_FVector(const EST_FVector &a): EST_TSimpleVector<float>(a) {}
  // Size constructor.
  EST_FVector(int n): EST_TSimpleVector<float>(n) {}
  
  // elementwise multiply
  EST_FVector &operator*=(const EST_FVector &s); 
  
  // elementwise add
  EST_FVector &operator+=(const EST_FVector &s); 
  
  // elementwise multiply by scalar
  EST_FVector &operator*=(float f); 
  
  // elementwise divide by scalar
  EST_FVector &operator/=(float f); 
  
  // save vector to file
  EST_write_status est_save(const EST_String &filename,
			    const EST_String &type);

  EST_write_status save(const EST_String &filename,
			const EST_String &type);
  
  // load vector from file
  EST_read_status load(const EST_String &filename);

  // Load from file in EST format
  EST_read_status est_load(const EST_String &filename);

  %extend {
    //    float max() const {
    //      return vector_max( *self );
    //    }

    void randomise( float scale ) {
      make_random_vector( *self, scale );
    }

    float sum() const {
      float sum = 0.0;
      int a_len = self->length();
  
      for( int i=0; i<a_len; ++i )
	sum += self->a_no_check(i);
      
      return sum;
    }
  }
};

// elementwise add
EST_FVector add(const EST_FVector &a,const EST_FVector &b);
// elementwise subtract
EST_FVector subtract(const EST_FVector &a,const EST_FVector &b);

%newobject sqrt;
%inline %{
  EST_FVector* sqrt( const EST_FVector &a ){
    int a_len = a.length();
    
    EST_FVector *ans = new EST_FVector(a_len);
    
    for( int i=0; i<a_len; ++i )
      ans->a_no_check(i) = sqrt( a.a_no_check(i) );
    
    return ans;
  }
%}

%newobject topower;
%inline %{
  EST_FVector* topower( const EST_FVector &a, float f ){
    int a_len = a.length();
    
    EST_FVector *ans = new EST_FVector(a_len);
    
    for( int i=0; i<a_len; ++i )
      ans->a_no_check(i) = pow( a.a_no_check(i), f );
    
    return ans;
  }
%}

EST_FVector operator-(const EST_FVector &a, const EST_FVector &b);
EST_FVector operator+(const EST_FVector &a, const EST_FVector &b);
// vector dot product
float operator*(const EST_FVector &v1, const EST_FVector &v2);



// least squares fit
bool polynomial_fit(EST_FVector &x, EST_FVector &y, 
		    EST_FVector &coefs, int order);

// weighted least squares fit
bool polynomial_fit(EST_FVector &x, EST_FVector &y, 
		    EST_FVector &coefs, 
		    EST_FVector &weights, int order);

float polynomial_value(const EST_FVector &coefs, const float x);

