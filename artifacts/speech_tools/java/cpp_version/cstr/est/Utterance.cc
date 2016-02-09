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
 /* Interface between java and C++ for EST_Utterance.                     */
 /*                                                                       */
 /*************************************************************************/


#include <stdio.h>
#include "jni_Utterance.h"
#include "ling_class/EST_Utterance.h"

static jobject utterance_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>0?s:-s; }

JNIEXPORT jboolean JNICALL
Java_cstr_est_Utterance_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  utterance_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "cpp_handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Utterance_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Utterance_create_1cpp_1utterance(JNIEnv *env, jobject self)
{
  EST_Utterance *utterance = new EST_Utterance;

  // printf("create utterance %p\n", utterance);

  env->SetLongField(self, handle_field, (jlong)utterance);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Utterance_destroy_1cpp_1utterance (JNIEnv *env, jobject self)
{
  EST_Utterance *utterance = (EST_Utterance *) env->GetLongField(self, handle_field);

  // printf("destroy utterance  %p\n", utterance);

  if (utterance)
    delete utterance;
  return 1;
}

JNIEXPORT jint JNICALL 
Java_cstr_est_Utterance_cpp_1num_1relations(JNIEnv *env, jobject self)
{
  EST_Utterance *utterance = (EST_Utterance *)env->GetLongField(self, handle_field);

  return utterance->num_relations();
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Utterance_cpp_1has_1relation(JNIEnv *env, 
					       jobject self,
					       jstring jname)
{
  EST_Utterance *utterance = (EST_Utterance *)env->GetLongField(self, handle_field);

  const char *name = env->GetStringUTFChars(jname, 0);

  bool has = utterance->relation_present(name);

  env->ReleaseStringUTFChars(jname, name);

  return has;
}

JNIEXPORT jlong JNICALL Java_cstr_est_Utterance_cpp_1relation_1n(JNIEnv *env, 
					       jobject self,
					       jint n)
{
  EST_Utterance *utterance = (EST_Utterance *)env->GetLongField(self, handle_field);

  EST_Relation * rel=NULL;

  EST_Features::Entries p;
  for(p.begin(utterance->relations); p && n>0; ++p,n--)
    ;
  
  if (n==0)
    rel = relation(p->v);

  return (jlong)rel;
}


JNIEXPORT jlong JNICALL 
Java_cstr_est_Utterance_cpp_1relation(JNIEnv *env, 
					       jobject self,
					       jstring jname)
{
  EST_Utterance *utterance = (EST_Utterance *)env->GetLongField(self, handle_field);

  const char *name = env->GetStringUTFChars(jname, 0);

  if (!utterance->relation_present(name))
    return (jlong)0l;

  EST_Relation * rel = utterance->relation(name);


  env->ReleaseStringUTFChars(jname, name);
  return (jlong)rel;
}


JNIEXPORT jlong JNICALL 
Java_cstr_est_Utterance_cpp_1create_1relation(JNIEnv *env, 
					   jobject self,
					   jstring jname)
{
    EST_Utterance *utterance = (EST_Utterance *)env->GetLongField(self, handle_field);

    const char *name = env->GetStringUTFChars(jname, 0);

    if (utterance->relation_present(name))
	return (jlong)0l;

    EST_Relation * rel = utterance->create_relation(name);


    env->ReleaseStringUTFChars(jname, name);
    return (jlong)rel;
}


JNIEXPORT jstring JNICALL 
Java_cstr_est_Utterance_cpp_1load (JNIEnv *env, jobject self, jstring jfilename)
{
  EST_Utterance *utterance = (EST_Utterance *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);

  EST_String fn(filename);
  EST_read_status stat = read_ok;

  CATCH_ERRORS()
    {
      env->ReleaseStringUTFChars(jfilename, filename);
      return env->NewStringUTF(EST_error_message);
    }

  stat = utterance->load(fn);

  END_CATCH_ERRORS();

  env->ReleaseStringUTFChars(jfilename, filename);

  const char *res = "";

  if (stat == read_format_error)
    res = "utterance format error";
  else if (stat != read_ok) 
    res = "utterance load error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Utterance_cpp_1save (JNIEnv *env, jobject self, jstring jfilename, jstring jformat)
{
  const EST_Utterance *utterance = (EST_Utterance *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *format = env->GetStringUTFChars(jformat, 0);
  const char *res = "";

  EST_write_status stat = format[0]=='\0'
    ? utterance->save(filename)
    : utterance->save(filename,format);

  env->ReleaseStringUTFChars(jfilename, filename);
  env->ReleaseStringUTFChars(jformat, format);

  if (stat == write_error) 
    res = "utterance save error";
  
  return  env->NewStringUTF(res);
}

