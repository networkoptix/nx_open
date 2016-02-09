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
 /* Interface between java and C++ for EST_Relation.                      */
 /*                                                                       */
 /*************************************************************************/


#include <stdio.h>
#include "jni_Relation.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_Item.h"

static jobject relation_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>0?s:-s; }

JNIEXPORT jboolean JNICALL
Java_cstr_est_Relation_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  relation_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "cpp_handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Relation_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Relation_create_1cpp_1relation(JNIEnv *env, 
						 jobject self,
						 jlong handle
						 )
{
  EST_Relation *relation =(handle == 0L
			   ? (new EST_Relation)
			   : (EST_Relation *)handle
			   );

  // printf("create relation %p\n", relation);

  env->SetLongField(self, handle_field, (jlong)relation);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Relation_destroy_1cpp_1relation (JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);

  // printf("destroy relation  %p\n", relation);

  delete relation;
  return 1;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Relation_cpp_1name(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  return  env->NewStringUTF(relation->name());
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Relation_cpp_1getFeature (JNIEnv *env, jobject self,
						jstring jn)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  const char *n = env->GetStringUTFChars(jn, 0);

  const char *v = (relation->f.present(n)
		   ? (const char *)relation->f.S(n)
		   : "");
  
  env->ReleaseStringUTFChars(jn, n);
  
  return env->NewStringUTF(v);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Relation_cpp_1type(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);

  EST_Item * hd = relation->head();

  const char *type;

  if (!hd)
    type = "empty";
  else
    {
    type = "linear";
    while (hd)
      {
	if (hd->up() || hd->down())
	  {
	    type = "tree";
	    break;
	  }
	hd=hd->next();
      }
    }

  return  env->NewStringUTF(type);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Relation_cpp_1load (JNIEnv *env, jobject self, jstring jfilename)
{
  EST_Relation *stream = (EST_Relation *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *res = "";

  EST_read_status stat = stream->load(filename);

  env->ReleaseStringUTFChars(jfilename, filename);

  if (stat == read_format_error)
    res = "stream format error";
  else if (stat == read_error) 
    res = "stream load error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Relation_cpp_1save (JNIEnv *env, jobject self, jstring jfilename, jstring jformat)
{
  EST_Relation *stream = (EST_Relation *) env->GetLongField(self, handle_field);

  const char *filename = env->GetStringUTFChars(jfilename, 0);
  const char *format = env->GetStringUTFChars(jformat, 0);
  const char *res = "";

  EST_write_status stat = stream->save(filename);

  env->ReleaseStringUTFChars(jfilename, filename);
  env->ReleaseStringUTFChars(jformat, format);

  if (stat == write_error) 
    res = "stream save error";
  
  return  env->NewStringUTF(res);
}

JNIEXPORT jlong JNICALL
Java_cstr_est_Relation_cpp_1head(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  EST_Item *item = relation->head();

  return (long)item;
}

JNIEXPORT jlong JNICALL
Java_cstr_est_Relation_cpp_1tail(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  EST_Item * item = relation->tail();

  return (long)item;
}

JNIEXPORT jlong JNICALL
Java_cstr_est_Relation_cpp_1append(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);

  EST_Item * item = relation->append();

  return (long)item;
}

JNIEXPORT void JNICALL 
Java_cstr_est_Relation_cpp_1removeItemList(JNIEnv *env, 
					   jobject self,
					   jlong ihandle
					   )
{  
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  EST_Item * item = (EST_Item *)ihandle;

  remove_item_list(relation, item);
}


JNIEXPORT jlong JNICALL
Java_cstr_est_Relation_cpp_1findItem(JNIEnv *env, jobject self, jfloat time)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  EST_Item * item = relation->head();

  while (item!=NULL && item->F("end", 0.0) < time)
    item = item->next();

  return (long)item;
}


JNIEXPORT jfloat JNICALL 
Java_cstr_est_Relation_cpp_1getEndTime(JNIEnv *env, jobject self)
{
  EST_Relation *relation = (EST_Relation *) env->GetLongField(self, handle_field);
  EST_Item *item = relation->tail();

  return  item?item->F("end",0.0):0.0;
}

