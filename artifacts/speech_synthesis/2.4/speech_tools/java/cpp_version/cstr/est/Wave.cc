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
 /*                                                                       */
 /*                                                                       */
 /*                 Author: Richard Caley <rjc@cstr.ed.ac.uk>             */
 /* -------------------------------------------------------------------   */
 /* Interface between java and C++ for EST_Wave.                          */
 /*                                                                       */
 /*************************************************************************/

#include <stdio.h>
#include <signal.h>
#include "jni_Wave.h"
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_audio.h"
#include "EST_inline_utils.h"

static jobject wave_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>=0?s:-s; }

static EST_Option wave_play_ops;

JNIEXPORT jboolean JNICALL
Java_cstr_est_Wave_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  wave_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "cpp_handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Wave_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Wave_create_1cpp_1wave(JNIEnv *env, jobject self)
{
  EST_Wave *wave = new EST_Wave;

  env->SetLongField(self, handle_field, (jlong)wave);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Wave_destroy_1cpp_1wave (JNIEnv *env, jobject self)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  delete wave;
  return 1;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Wave_cpp_1name (JNIEnv *env, jobject self)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return  env->NewStringUTF(wave->name());
}

JNIEXPORT void JNICALL 
Java_cstr_est_Wave_cpp_1setName (JNIEnv *env, jobject self, jstring jname)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
  const char *name=env->GetStringUTFChars(jname, 0);

  wave->set_name(name);

  env->ReleaseStringUTFChars(jname, name);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Wave_cpp_1load (JNIEnv *env, jobject self, jstring jfilename)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *res = "";

  EST_read_status stat = wave->load(filename);

  env->ReleaseStringUTFChars(jfilename, filename);

  if (stat == read_format_error)
    res = "wave format error";
  else if (stat == read_error) 
    res = "wave load error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Wave_cpp_1save (JNIEnv *env, jobject self, jstring jfilename, jstring jformat)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *format = env->GetStringUTFChars(jformat, 0);
  const char *res = "";

  EST_write_status stat = wave->save(filename, format);

  env->ReleaseStringUTFChars(jfilename, filename);
  env->ReleaseStringUTFChars(jformat, format);

  if (stat == write_error) 
    res = "wave save error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT void JNICALL 
Java_cstr_est_Wave_cpp_1resample (JNIEnv *env, jobject self, jint rate)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  wave->resample(rate);
}

JNIEXPORT void JNICALL 
Java_cstr_est_Wave_cpp_1set_1play_1ops(JNIEnv *env, jclass myclass,
					   jstring jprotocol,
					   jstring jcommand,
					   jstring jserver
					   )
{
  (void)myclass;
  const char *protocol = env->GetStringUTFChars(jprotocol, 0);
  const char *command = env->GetStringUTFChars(jcommand, 0);
  const char *server = env->GetStringUTFChars(jserver, 0);

  if (*protocol)
    wave_play_ops.add_item("-p",protocol);
  if (*command)
    wave_play_ops.add_item("-command",command);
  if (*server)
    wave_play_ops.add_item("-display",server);
  wave_play_ops.add_item("-otype","riff");
  
  env->ReleaseStringUTFChars(jprotocol, protocol);
  env->ReleaseStringUTFChars(jcommand, command);
  env->ReleaseStringUTFChars(jserver, server);
}

JNIEXPORT void JNICALL 
Java_cstr_est_Wave_cpp_1play (JNIEnv *env, jobject self, 
				  jfloat start_t, jfloat end_t)
{
   EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
   EST_Wave wv;

   int start = (int)(start_t * wave->sample_rate() + 0.5);
   int end = (int)(end_t * wave->sample_rate() + 0.5);

   wave_subwave(wv, *wave, start, end-start);
   // EST_write_status st = wv.save("/tmp/est_java_wave", "nist");
   
   play_wave(wv, wave_play_ops);

}

JNIEXPORT void JNICALL 
Java_cstr_est_Wave_cpp_1play_1all (JNIEnv *env, jobject self)
{
   EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

   play_wave(*wave, wave_play_ops);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_cpp_1num_1channels(JNIEnv *env, jobject self)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return wave->num_channels();
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_cpp_1num_1samples(JNIEnv *env, jobject self)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return wave->num_samples();
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_cpp_1sample_1rate(JNIEnv *env, jobject self)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return wave->sample_rate();
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_cpp_1amplitude(JNIEnv *env, jobject self,
				      jint c)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
  short a=0;

  for(int i=0; i<wave->num_samples(); i++)
    {
      short p = wave->a(i,c);
      if (abs(p) > a)
	a=abs(p);
    }

  return a;
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_a__II(JNIEnv *env, jobject self,
				  jint x,
				  jint c)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return wave->a(x,c);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Wave_a__FI(JNIEnv *env, jobject self,
		                  jfloat t,
				  jint c)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);

  return wave->a(irint(t*wave->sample_rate()),c);
}

JNIEXPORT void JNICALL
Java_cstr_est_Wave_cpp_1getScanlines(JNIEnv *env, jobject self, 
					 jint c,
					 jbyteArray line, 
					 jint lstart, jint lnum,
					 jint xoff, jint chunk,
					 jint width, jint height, 
					 jint amp)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
  jbyte *pixels = env->GetByteArrayElements(line, 0);
  int num_samples = wave->num_samples();
  short *data = wave->values().memory();
  int channels = wave->num_channels();

  for(int l=lstart; l<lstart+lnum; l++)
    {
      int min = (int)((float)(height - l)*amp*2.0/height-amp+0.5);
      int max = (int)((float)(height - l+1)*amp*2.0/height-amp+0.5);

      jbyte *lpixels = pixels + (l-lstart)*chunk;

      short la=0;
      int from=(int)((float)xoff*num_samples/width+0.5);

      for(int i=xoff; i<xoff+chunk; i++)
	{
	  int fl=0;
	  int to = (int)((i+1.0)*num_samples/width+0.5);
	  for(int j=from; j<to; j++)
	    {
	      short a = data[j*channels + c];
	      if ((a>min && la <=max)|| (la > min && a <=max))
		fl++;
	      la=a;
	    }
	  lpixels[i-xoff] = fl<5?fl:4;
	  from=to;
	}
    }
}

JNIEXPORT jint JNICALL
Java_cstr_est_Wave_cpp_1getMin(JNIEnv *env, jobject self, 
				   jint c, jint x1, jint x2)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
  int minv=64000;

  if (x1<0) x1 = 0;
  if (x2>wave->num_samples()) x2 =  wave->num_samples();

  for(int x=x1; x<x2; x++)
    {
      short v = wave->a(x,c);
      if (v<minv)
	minv=v;
    }
  return minv;
}

JNIEXPORT jint JNICALL
Java_cstr_est_Wave_cpp_1getMax(JNIEnv *env, jobject self, 
				   jint c, jint x1, jint x2)
{
  EST_Wave *wave = (EST_Wave *) env->GetLongField(self, handle_field);
  int maxv=-64000;

  if (x1<0) x1 = 0;
  if (x2>wave->num_samples()) x2 =  wave->num_samples();

  for(int x=x1; x<x2; x++)
    {
      short v = wave->a(x, c);
      if (v>maxv)
	maxv=v;
    }
  return maxv;
}

