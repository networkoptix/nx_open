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
 /*                 Author: Richard Caley <rjc@cstr.ed.ac.uk>             */
 /* -------------------------------------------------------------------   */
 /* Interface Between Java And C++ For Est_Track.                         */
 /*                                                                       */
 /*************************************************************************/


#include <stdio.h>
#include "jni_Track.h"
#include "EST_Track.h"
#include "EST_track_aux.h"

static jobject track_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>0?s:-s; }

static jint frame_before(EST_Track *track, jfloat t)
{
  if (track->equal_space())
    {
      float s = track->shift();
      int f = (int)(t/s)-1;
      if (f<0)
	f=0;
      // fprintf(stderr, "fb t=%f s=%f f=%d\n", t, s, f);
      return f;
    }
  else
    {
      int f=0;

      for(int i=0; i<track->num_frames(); f=i++)
	if (track->t(i)>=t)
	  break;
      return f;
    }
}

static jint frame_after(EST_Track *track, jfloat t)
{
  if (track->equal_space())
    {
      float s = track->shift();
      int f = (int)(t/s)+1;
      if (f>track->num_frames())
	f=track->num_frames();
      // fprintf(stderr, "fa t=%f s=%f f=%d\n", t, s, f);
      return f;
    }
  else
    {
      int i;
      for(i=0; i<track->num_frames(); i++)
	if (track->t(i)>=t)
	  break;
      
      return i;
    }
}

JNIEXPORT jboolean JNICALL
Java_cstr_est_Track_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  track_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "cpp_handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Track_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Track_create_1cpp_1track(JNIEnv *env, jobject self)
{
  EST_Track *track = new EST_Track;

  // printf("create track %p\n", track);

  env->SetLongField(self, handle_field, (jlong)track);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Track_destroy_1cpp_1track (JNIEnv *env, jobject self)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  // printf("destroy track  %p\n", track);

  delete track;
  return 1;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Track_cpp_1name (JNIEnv *env, jobject self)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return  env->NewStringUTF(track->name());
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Track_cpp_1load (JNIEnv *env, jobject self, jstring jfilename)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *res = "";

  EST_read_status stat = track->load(filename);

  env->ReleaseStringUTFChars(jfilename, filename);

  if (stat == read_format_error)
    res = "track format error";
  else if (stat == read_error) 
    res = "track load error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Track_cpp_1save (JNIEnv *env, jobject self, 
				   jstring jfilename, jstring jformat,
				   jfloat start,
				   jfloat end)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *format = env->GetStringUTFChars(jformat, 0);
  const char *res = "";
  
  int sframe = frame_before(track, start);
  int eframe = frame_after(track, end);

  EST_Track segment;

  track->sub_track(segment,
		   sframe, eframe-sframe,
		   0, EST_ALL);
		   

  EST_write_status stat = segment.save(filename, format);

  env->ReleaseStringUTFChars(jfilename, filename);
  env->ReleaseStringUTFChars(jformat, format);

  if (stat == write_error) 
    res = "track save error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1num_1frames(JNIEnv *env, jobject self)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->num_frames();
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1num_1channels(JNIEnv *env, jobject self)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->num_channels();
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Track_cpp_1getEndTime(JNIEnv *env, jobject self)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);
  int f = track->num_frames()-1;

  return f>=0?track->t(f):0.0;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Track_cpp_1channelName(JNIEnv *env, jobject self,
					 jint i)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);
  EST_String cname(i>=0 && i < track->num_channels()
		   ?track->channel_name(i)
		   :EST_String::Empty);

  return  env->NewStringUTF(cname);
}


JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1channelPosition(JNIEnv *env, jobject self,
					     jstring jname)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);
  const char *name = env->GetStringUTFChars(jname, 0);

  int p = track->channel_position(name);
  
  env->ReleaseStringUTFChars(jname, name);

  return  p;
}


JNIEXPORT jfloat JNICALL 
Java_cstr_est_Track_a__II(JNIEnv *env, jobject self,
			       jint f, jint c)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->a((int)f,(int)c);
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Track_a__FI(JNIEnv *env, jobject self,
			       jfloat t, jint c)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->a((float)t,(int)c);
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Track_cpp_1t(JNIEnv *env, jobject self,
			       jint f)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->t((int)f);
}


JNIEXPORT jboolean JNICALL 
Java_cstr_est_Track_cpp_1val(JNIEnv *env, jobject self,
				 jint f)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->val((int)f);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1frameBefore(JNIEnv *env, jobject self,
					 jfloat t)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return frame_before(track, t);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1frameAfter(JNIEnv *env, jobject self,
					jfloat t)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return frame_after(track, t);
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Track_cpp_1frameNearest(JNIEnv *env, jobject self,
					  jfloat t)
{
  EST_Track *track = (EST_Track *) env->GetLongField(self, handle_field);

  return track->index(t);
}


