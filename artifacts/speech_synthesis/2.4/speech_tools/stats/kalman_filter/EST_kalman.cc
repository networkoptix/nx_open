/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1995,1996                          */
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
/*                   Author :  Simon King                                */
/*                   Date   :  June 1998                                 */
/*-----------------------------------------------------------------------*/
/*              Kalman filtering, i.e. linear model fitting              */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include "EST.h"
#include "EST_kalman.h"

static bool kalman_filter_param_check(EST_FVector &x,
				      EST_FMatrix &P,
				      EST_FMatrix &Q,
				      EST_FMatrix &R,
				      EST_FMatrix &A,
				      EST_FMatrix &H,
				      EST_FVector &z);



bool kalman_filter(EST_FVector &x,
		   EST_FMatrix &P,
		   EST_FMatrix &Q,
		   EST_FMatrix &R,
		   EST_FMatrix &A,
		   EST_FMatrix &H,
		   EST_FVector &z)
{
    // see kalman.h for meaning of args

    if(!kalman_filter_param_check(x,P,Q,R,A,H,z))
    {
	cerr << "Kalman filter parameters inconsistent !" << endl;
	return FALSE;
    }

    EST_FMatrix K,I,At,Ht,PHt,HPHt_R,HPHt_R_inv;
    int state_dim=x.length();
    int singularity;
    eye(I,state_dim);
    transpose(A,At);

    cerr << "predict" << endl;

    // predict
    // =======
    // if A is the identity matrix we could speed this up a LOT
    x = A * x;
    P = A * P * At + Q;

    cerr << "correct" << endl;


    // correct
    // =======
    //        T        T      -1
    // K = P H  ( H P H  + R )
    transpose(H,Ht);
    PHt = P * Ht;
    HPHt_R=(H * PHt) + R;

    if(!inverse( HPHt_R , HPHt_R_inv, singularity))
    {
	if(singularity != -1)
	{
	    cerr << " H * P * Ht + R is singular !" << endl;
	    return FALSE;
	}
	cerr << "Matrix inversion failed for an unknown reason !" << endl;
	return FALSE;
    }

    K = PHt * HPHt_R_inv;
    x = add(x, K * subtract(z,H * x));
    P = (I - K * H) * P;

    // try and remedy numerical errors
    symmetrize(P);

    //cerr << "done" << endl;

    return TRUE;
}



bool kalman_filter_Pinv(EST_FVector &x,
			EST_FMatrix &Pinv,
			EST_FMatrix &Q,
			EST_FMatrix &Rinv,
			EST_FMatrix &A,
			EST_FMatrix &H,
			EST_FVector &z)
{

    // a different formulation, using the inverse
    // covariance matrix, and a more stable update
    // equation

    // from: 
    // Intro. to Random Signals and Kalman Applied Filtering
    // Brown & Hwang (Wiley,1997)
    // p. 248

    if(!kalman_filter_param_check(x,Pinv,Q,Rinv,A,H,z))
    {
	cerr << "Kalman filter parameters inconsistent !" << endl;
	return FALSE;
    }


    EST_FMatrix K,I,At,Ht,P;
    int singularity;
    int state_dim=x.length();
    eye(I,state_dim);
    transpose(A,At);
    transpose(H,Ht);

    cerr << "Compute P" << endl;


    // update error covariance
    // =======================
    Pinv = Pinv + (Ht * Rinv * H);
    
    if(!inverse(Pinv,P,singularity))
    {
	if(singularity != -1)
	{
	    cerr << "P is singular !" << endl;
	    return FALSE;
	}
	cerr << "Matrix inversion failed for an unknown reason !" << endl;
	return FALSE;
    }

    // compute gain
    // ============
    K = P * Ht * Rinv;

    // update state
    // ============
    x = add(x, K * subtract(z,H*x));

    // project ahead
    // =============
    x = A * x;
    P = A * P * At + Q;
    if(!inverse(P,Pinv,singularity))
    {
	if(singularity != -1)
	{
	    cerr << "Pinv is singular !" << endl;
	    return FALSE;
	}
	cerr << "Matrix inversion failed for an unknown reason !" << endl;
	return FALSE;
    }

    // try and remedy numerical errors
    //symmetrize(P);

    //cerr << "done" << endl;

    return TRUE;

}



bool kalman_filter_param_check(EST_FVector &x,
			       EST_FMatrix &P,
			       EST_FMatrix &Q,
			       EST_FMatrix &R,
			       EST_FMatrix &A,
			       EST_FMatrix &H,
			       EST_FVector &z)
{


    int state_dim=x.length();
    int measurement_dim=z.length();


    // sanity checks
    if((state_dim <= 0) || 
       (measurement_dim <= 0))
    {
	cerr << "No state or measurements !!" << endl;
	return FALSE;
    }

    // dimensionality

    // P is error covariance
    if((P.num_rows() != state_dim) ||
       (P.num_columns() != state_dim) )
    {
	cerr << "P, or Pinv, must be a symmetrical square matrix of the same dimension" << endl;
	cerr << "as the state vector, x" << endl;
	return FALSE;
    }

    // Q is process noise covariance
    if((Q.num_rows() != state_dim) ||
       (Q.num_columns() != state_dim) )
    {
	cerr << "Q must be a symmetrical square matrix of the same dimension" << endl;
	cerr << "as the state vector, x" << endl;
	return FALSE;
    }

    // R is measurement noise covariance
    if((R.num_rows() != measurement_dim) ||
       (R.num_columns() != measurement_dim) )
    {
	cerr << "R, or Rinv, must be a symmetrical square matrix of the same dimension" << endl;
	cerr << "as the measurement vector, z" << endl;
	return FALSE;
    }

    if((A.num_rows() != state_dim) ||
       (A.num_columns() != state_dim) )
    {
	cerr << "A must be a square matrix of the same dimension" << endl;
	cerr << "as the state vector, x" << endl;
	return FALSE;
    }

    if((H.num_rows() != measurement_dim) ||
       (H.num_columns() != state_dim) )
    {
	cerr << "H must have dimensions to fit  z = Hx" << endl;
	return FALSE;
    }

    return TRUE;
}
