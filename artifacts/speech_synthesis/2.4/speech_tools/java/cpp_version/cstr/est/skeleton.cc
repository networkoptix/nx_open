/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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

#include <stdio.h>
#include "jni_Skeleton.h"
#include "ling_class/EST_Skeleton.h"

static jobject skeleton_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>0?s:-s; }

JNIEXPORT jboolean JNICALL
Java_cstr_est_Skeleton_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  skeleton_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Skeleton_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Skeleton_create_11skeleton(JNIEnv *env, jobject self)
{
  EST_Skeleton *skeleton = new EST_Skeleton;

  // printf("create skeleton %p\n", skeleton);

  env->SetLongField(self, handle_field, (jlong)skeleton);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Skeleton_destroy_11skeleton (JNIEnv *env, jobject self)
{
  EST_Skeleton *skeleton = (EST_Skeleton *) env->GetLongField(self, handle_field);

  // printf("destroy skeleton  %p\n", skeleton);

  delete skeleton;
  return 1;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Skeleton_1name(JNIEnv *env, jobject self)
{
  EST_Skeleton *skeleton = (EST_Skeleton *) env->GetLongField(self, handle_field);
  return  env->NewStringUTF(skeleton->name());
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Skeleton_1load (JNIEnv *env, jobject self, jstring jfilename)
{
  EST_Skeleton *skeleton = (EST_Skeleton *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *res = "";

  EST_read_status stat = skeleton->load(filename);

  env->ReleaseStringUTFChars(jfilename, filename);

  if (stat == read_format_error)
    res = "skeleton format error";
  else if (stat == read_error) 
    res = "skeleton load error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Skeleton_1save (JNIEnv *env, jobject self, jstring jfilename, jstring jformat)
{
  const EST_Skeleton *skeleton = (EST_Skeleton *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *format = env->GetStringUTFChars(jformat, 0);
  const char *res = "";

  EST_write_status stat = skeleton->save(filename,format);

  env->ReleaseStringUTFChars(jfilename, filename);
  env->ReleaseStringUTFChars(jformat, format);

  if (stat == write_error) 
    res = "skeleton save error";
  
  return  env->NewStringUTF(res);
}

