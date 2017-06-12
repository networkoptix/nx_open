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
 /* --------------------------------------------------------------------  */
 /* Interface to signal processing code.                                  */
 /*                                                                       */
 /*************************************************************************/


#include <stdio.h>
#include <signal.h>
#include "jni_Sigpr.h"
#include "sigpr/EST_spectrogram.h"
#include "sigpr/EST_misc_sigpr.h"

JNIEXPORT void JNICALL
Java_cstr_est_Sigpr_cpp_1spectrogram(JNIEnv *env, jclass myclass,
					 jlong waveh, jlong trackh,
					 jfloat shift, 
					 jfloat length,
					 jint order,
					 jfloat floor,
					 jfloat ceil)
{
  (void)env;
  (void)myclass;
  EST_Wave *wave = (EST_Wave *)waveh; 
  EST_Track *sg = (EST_Track *)trackh;

  EST_Wave pwave;

  EST_pre_emphasis(*wave, pwave, 0.94);
  
  raw_spectrogram(*sg, pwave, length, shift, order);

  scale_spectrogram(*sg, 1.0, floor, ceil);
}
