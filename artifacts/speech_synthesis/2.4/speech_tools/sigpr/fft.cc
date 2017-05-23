/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                       Author :  Simon King (taken from Tony Robinson) */
/*                       Date   :  July 1994                             */
/*-----------------------------------------------------------------------*/
/*                         FFT functions                                 */
/*                                                                       */
/*=======================================================================*/

#include <cmath>
//#include <iostream>
//#include <fstream>
#include "sigpr/EST_fft.h"
#include "EST_math.h"
#include "EST_error.h"

#define PI8 0.392699081698724 /* PI / 8.0 */
#define RT2 1.4142135623731  /* sqrt(2.0) */
#define IRT2 0.707106781186548  /* 1.0/sqrt(2.0) */

static void FR2TR(int, float*, float*);
static void FR4TR(int, int, float*, float*, float*, float*);
static void FORD1(int, float*);
static void FORD2(int, float*);

/*
** FAST(b,n)
** This routine replaces the float vector b
** of length n with its finite discrete fourier transform.
** DC term is returned in b[0]; 
** n/2th harmonic float part in b[1].
** jth harmonic is returned as complex number stored as
** b[2*j] + i b[2*j + 1] 
** (i.e., remaining coefficients are as a DPCOMPLEX vector).
** 
*/


static int slowFFTsub(EST_FVector &real, EST_FVector &imag, float f) 
{
    // f = -1 for FFT, 1 for IFFT
    // would be nicer if we used a complex number class, 
    // but we don't, so it isn't

    // taken from the FORTRAN old chestnut
    // in various sig proc books
    // FORTRAN uses 1..n arrays, so subtract 1 all over the place


    float u_real,u_imag;
    float w_real,w_imag;
    float t_real,t_imag;
    float tmp_real,tmp_imag;

    int M,N;
    int i,j,k,l;
    
    M = fastlog2(real.n());
    N = (int)pow(float(2.0),(float)M);

    if (N != real.n())
      {
	EST_warning("Illegal FFT order %d", real.n());
	return -1;
      }

    for(l=1;l<=M;l++){

      int le = (int)pow(float(2.0),(float)(M+1-l));
	int le1=le/2;

	u_real = 1.0;
	u_imag = 0.0;

	w_real=cos(PI/le1);
	w_imag=f * sin(PI/le1);

	for (j=1;j<=le1;j++)
	{
	    for (i=j;i<=N-le1;i+=le)
	    {
		int ip=i+le1;
		t_real = real.a_no_check(i-1) + real.a_no_check(ip-1);
		t_imag = imag.a_no_check(i-1) + imag.a_no_check(ip-1);

		tmp_real = real.a_no_check(i-1) - real.a_no_check(ip-1);
		tmp_imag = imag.a_no_check(i-1) - imag.a_no_check(ip-1);

		real.a_no_check(ip-1) = tmp_real*u_real - tmp_imag*u_imag;
		imag.a_no_check(ip-1) = tmp_real*u_imag + tmp_imag*u_real;

		real.a_no_check(i-1) = t_real;
		imag.a_no_check(i-1) = t_imag;
	    }

	    tmp_real = u_real*w_real - u_imag*w_imag;
	    tmp_imag = u_real*w_imag + u_imag*w_real;
	    
	    u_real=tmp_real;
	    u_imag=tmp_imag;

	}

    }


    int NV2=N/2;
    int NM1=N-1;
    j=1;


    for (i=1; i<=NM1;i++)
    {
	if (i < j)
	{
	    t_real=real(j-1);
	    t_imag=imag(j-1);
	    
	    real[j-1] = real(i-1);
	    imag[j-1] = imag(i-1);

	    real[i-1] = t_real;
	    imag[i-1] = t_imag;
	    
	}

	k=NV2;

	while(k < j)
	{
	    j=j-k;
	    k=k/2;
	}

	j=j+k;

    }

    return 0;
}


int slowFFT(EST_FVector &real, EST_FVector &imag) 
{
    return slowFFTsub(real,imag,-1.0);
}


int slowIFFT(EST_FVector &real, EST_FVector &imag) 
{
    int N=real.n();
    if (N <=0 )
	return -1;

    if (slowFFTsub(real,imag,1.0) != 0)
	return -1;

    for(int i=1;i<=N;i++){
	real[i-1] /= (float)N;
	imag[i-1] /= (float)N;
    }

    return 0;
}


int energy_spectrum(EST_FVector &real, EST_FVector &imag)
{
    if (slowFFT(real, imag) != 0)
	return -1;

    int i;
    for(i=0;i<real.n();i++)
	real[i] = imag[i] = (real(i)*real(i) + imag(i)*imag(i));
    
       return 0;
}

int power_spectrum_slow(EST_FVector &real, EST_FVector &imag)
{

    if (slowFFT(real,imag) != 0)
	return -1;

    int i;
    for(i=0;i<real.n();i++)
	real[i] = imag[i] = sqrt((real(i)*real(i) + imag(i)*imag(i)));
    
       return 0;
}

int power_spectrum(EST_FVector &real, EST_FVector &imag)
{

    if (fastFFT(real) == 0)
	return -1;

    int i,j,k;
    int n=real.n();
    for(i=0,j=0, k=1;i<n;i+=2,j++,k+=2)
	real.a_no_check(j) 
	  = imag.a_no_check(j) 
	  = sqrt((real.a_no_check(i)*real.a_no_check(i) 
		  + real.a_no_check(k)*real.a_no_check(k)));
    
       return 0;
}

// the following code is not by Simon King, as you can see
/*
** Discrete Fourier analysis routine
** from IEEE Programs for Digital Signal Processing
** G. D. Bergland and M. T. Dolan, original authors
** Translated from the FORTRAN with some changes by Paul Kube
**
** Modified to return the power spectrum by Chuck Wooters
**
** Modified again by Tony Robinson (ajr@eng.cam.ac.uk) Dec 92
**
** Modified to use EST_FVector class Simon King (simonk@cstr.ed.ac.uk) Nov 96
**
*/

#define signum(i) (i < 0 ? -1 : i == 0 ? 0 : 1)

int fastFFT(EST_FVector &invec) 
{
    // Tony Robinsons
    float fn;
    int i, in, nn, n2pow, n4pow, nthpo;
    
    // we could modify all the code to use vector classes ....
    // ... or we could do this:

    // TO DO
    // use FSimpleVector::copy_section here

    // quick fix
    int n=invec.n(); // order

#if 0    
    float *b = new float[n];
    for(i=0; i<n; i++)
	b[i] = invec(i);
#endif
    float *b=invec.memory();

    n2pow = fastlog2(n);
    if (n2pow <= 0) return 0;
    nthpo = n;
    fn = nthpo;
    n4pow = n2pow / 2;
    
    /* radix 2 iteration required; do it now */  
    if (n2pow % 2) 
    {
	nn = 2;
	in = n / nn;
	FR2TR(in, b, b + in );
    }
    else nn = 1;
    
    /* perform radix 4 iterations */
    for(i = 1; i <= n4pow; i++) {
	nn *= 4;
	in = n / nn;
	FR4TR(in, nn, b, b + in, b + 2 * in, b + 3 * in);
    }
    
    /* perform inplace reordering */
    FORD1(n2pow, b);
    FORD2(n2pow, b);
    
    /* take conjugates */
    for(i = 3; i < n; i += 2) b[i] = -b[i];

#if 0    
    // copy back
    for(i=0; i<n; i++)
	invec[i] = b[i];
#endif
    
    return 1;
}

/* radix 2 subroutine */
void FR2TR(int in, float *b0, float *b1) 
{
    int k;
    float t;
    for (k = 0; k < in; k++) 
    {
	t = b0[k] + b1[k];
	b1[k] = b0[k] - b1[k];
	b0[k] = t;
    }
}

/* radix 4 subroutine */
void FR4TR(int in, int nn, float *b0, float *b1, float *b2, float* b3) {
  float arg, piovn, th2;
  float *b4 = b0, *b5 = b1, *b6 = b2, *b7 = b3;
  float t0, t1, t2, t3, t4, t5, t6, t7;
  float r1, r5, pr, pi;
  float c1, c2, c3, s1, s2, s3;
  
  int j, k, jj, kk, jthet, jlast, ji, jl, jr, int4;
  int L[16], L1, L2, L3, L4, L5, L6, L7, L8, L9, L10, L11, L12, L13, L14, L15;
  int j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14;
  int k0, kl;
  
  L[1] = nn / 4;	
  for(k = 2; k < 16; k++) {  /* set up L's */
    switch (signum(L[k-1] - 2)) {
    case -1:
      L[k-1]=2;
    case 0:
      L[k]=2;
      break;
    case 1:
      L[k]=L[k-1]/2;
    }
  }

  L15=L[1]; L14=L[2]; L13=L[3]; L12=L[4]; L11=L[5]; L10=L[6]; L9=L[7];
  L8=L[8];  L7=L[9];  L6=L[10]; L5=L[11]; L4=L[12]; L3=L[13]; L2=L[14];
  L1=L[15];

  piovn = PI / nn;
  ji=3;
  jl=2;
  jr=2;
  
  for(j1=2;j1<=L1;j1+=2)
  for(j2=j1;j2<=L2;j2+=L1)
  for(j3=j2;j3<=L3;j3+=L2)
  for(j4=j3;j4<=L4;j4+=L3)
  for(j5=j4;j5<=L5;j5+=L4)
  for(j6=j5;j6<=L6;j6+=L5)
  for(j7=j6;j7<=L7;j7+=L6)
  for(j8=j7;j8<=L8;j8+=L7)
  for(j9=j8;j9<=L9;j9+=L8)
  for(j10=j9;j10<=L10;j10+=L9)
  for(j11=j10;j11<=L11;j11+=L10)
  for(j12=j11;j12<=L12;j12+=L11)
  for(j13=j12;j13<=L13;j13+=L12)
  for(j14=j13;j14<=L14;j14+=L13)
  for(jthet=j14;jthet<=L15;jthet+=L14) 
    {
      th2 = jthet - 2;
      if(th2<=0.0) 
	{
	  for(k=0;k<in;k++) 
	    {
	      t0 = b0[k] + b2[k];
	      t1 = b1[k] + b3[k];
	      b2[k] = b0[k] - b2[k];
	      b3[k] = b1[k] - b3[k];
	      b0[k] = t0 + t1;
	      b1[k] = t0 - t1;
	    }
	  if(nn-4>0) 
	    {
	      k0 = in*4 + 1;
	      kl = k0 + in - 1;
	      for (k=k0;k<=kl;k++) 
		{
		  kk = k-1;
		  pr = IRT2 * (b1[kk]-b3[kk]);
		  pi = IRT2 * (b1[kk]+b3[kk]);
		  b3[kk] = b2[kk] + pi;
		  b1[kk] = pi - b2[kk];
		  b2[kk] = b0[kk] - pr;
		  b0[kk] = b0[kk] + pr;
		}
	    }
	}
      else 
	{
	  arg = th2*piovn;
	  c1 = cos(arg);
	  s1 = sin(arg);
	  c2 = c1*c1 - s1*s1;
	  s2 = c1*s1 + c1*s1;
	  c3 = c1*c2 - s1*s2;
	  s3 = c2*s1 + s2*c1;

	  int4 = in*4;
	  j0=jr*int4 + 1;
	  k0=ji*int4 + 1;
	  jlast = j0+in-1;
	  for(j=j0;j<=jlast;j++) 
	    {
	      k = k0 + j - j0;
	      kk = k-1; jj = j-1;
	      r1 = b1[jj]*c1 - b5[kk]*s1;
	      r5 = b1[jj]*s1 + b5[kk]*c1;
	      t2 = b2[jj]*c2 - b6[kk]*s2;
	      t6 = b2[jj]*s2 + b6[kk]*c2;
	      t3 = b3[jj]*c3 - b7[kk]*s3;
	      t7 = b3[jj]*s3 + b7[kk]*c3;
	      t0 = b0[jj] + t2;
	      t4 = b4[kk] + t6;
	      t2 = b0[jj] - t2;
	      t6 = b4[kk] - t6;
	      t1 = r1 + t3;
	      t5 = r5 + t7;
	      t3 = r1 - t3;
	      t7 = r5 - t7;
	      b0[jj] = t0 + t1;
	      b7[kk] = t4 + t5;
	      b6[kk] = t0 - t1;
	      b1[jj] = t5 - t4;
	      b2[jj] = t2 - t7;
	      b5[kk] = t6 + t3;
	      b4[kk] = t2 + t7;
	      b3[jj] = t3 - t6;
	    }
	  jr += 2;
	  ji -= 2;
	  if(ji-jl <= 0) {
	    ji = 2*jr - 1;
	    jl = jr;
	  }
	}
    }
}

/* an inplace reordering subroutine */
void FORD1(int m, float *b) {
    int j, k = 4, kl = 2, n = 0x1 << m;
    float t;
    
    for(j = 4; j <= n; j += 2) {
	if (k - j>0) {
	    t = b[j-1];
	    b[j - 1] = b[k - 1];
	    b[k - 1] = t;
	}
	k -= 2;
	if (k - kl <= 0) {
	    k = 2*j;
	    kl = j;
	}
    }	
}

/*  the other inplace reordering subroutine */
void FORD2(int m, float *b) 
{
  float t;
  
  int n = 0x1<<m, k, ij, ji, ij1, ji1;
  
  int l[16], l1, l2, l3, l4, l5, l6, l7, l8, l9, l10, l11, l12, l13, l14, l15;
  int j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14;
  

  l[1] = n;
  for(k=2;k<=m;k++) l[k]=l[k-1]/2;
  for(k=m;k<=14;k++) l[k+1]=2;
  
  l15=l[1];l14=l[2];l13=l[3];l12=l[4];l11=l[5];l10=l[6];l9=l[7];
  l8=l[8];l7=l[9];l6=l[10];l5=l[11];l4=l[12];l3=l[13];l2=l[14];l1=l[15];

  ij = 2;
  
  for(j1=2;j1<=l1;j1+=2)
  for(j2=j1;j2<=l2;j2+=l1)
  for(j3=j2;j3<=l3;j3+=l2)
  for(j4=j3;j4<=l4;j4+=l3)
  for(j5=j4;j5<=l5;j5+=l4)
  for(j6=j5;j6<=l6;j6+=l5)
  for(j7=j6;j7<=l7;j7+=l6)
  for(j8=j7;j8<=l8;j8+=l7)
  for(j9=j8;j9<=l9;j9+=l8)
  for(j10=j9;j10<=l10;j10+=l9)
  for(j11=j10;j11<=l11;j11+=l10)
  for(j12=j11;j12<=l12;j12+=l11)
  for(j13=j12;j13<=l13;j13+=l12)
  for(j14=j13;j14<=l14;j14+=l13)
  for(ji=j14;ji<=l15;ji+=l14) {
    ij1 = ij-1; ji1 = ji - 1;
    if(ij-ji<0) {
      t = b[ij1-1];
      b[ij1-1]=b[ji1-1];
      b[ji1-1] = t;
	
      t = b[ij1];
      b[ij1]=b[ji1];
      b[ji1] = t;
    }
    ij += 2;
  }
} 

int fastlog2(int n) {
    int num_bits, power = 0;
    
    if ((n < 2) || (n % 2 != 0)) return(0);
    num_bits = sizeof(int) * 8;	/* How big are ints on this machine? */
    
    while(power <= num_bits) {
	n >>= 1;
	power += 1;
	if (n & 0x01) {
	    if (n > 1)	return(0);
	    else return(power);
	}
    }
    return(0);
}
