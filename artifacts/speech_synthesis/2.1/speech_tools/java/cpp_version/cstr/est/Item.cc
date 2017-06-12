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
 /* Interface between java and C++ for EST_Item.                          */
 /*                                                                       */
 /*************************************************************************/


#include <stdio.h>
#include "jni_Item.h"
#include "ling_class/EST_Item.h"
#include "ling_class/EST_item_aux.h"

static jobject item_class;
static jfieldID handle_field;

static inline short abs(short s) { return s>0?s:-s; }

JNIEXPORT jboolean JNICALL
Java_cstr_est_Item_initialise_1cpp (JNIEnv *env, jclass myclass)
{
  item_class = env->NewGlobalRef(myclass);
  handle_field = env->GetFieldID(myclass, "cpp_handle", "J");

  if (!handle_field)
    {
    printf("can't find handle!\n");
    return 0;
    }

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Item_finalise_1cpp (JNIEnv *env, jclass myclass)
{
  (void)env;
  (void)myclass;
  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Item_create_1cpp_1item(JNIEnv *env, 
						 jobject self,
						 jlong handle
						 )
{
  EST_Item *item =(handle == 0L
			   ? (new EST_Item)
			   : (EST_Item *)handle
			   );

  // printf("create item %p\n", item);

  env->SetLongField(self, handle_field, (jlong)item);

  return 1;
}

JNIEXPORT jboolean JNICALL 
Java_cstr_est_Item_destroy_1cpp_1item (JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  // printf("destroy item  %p\n", item);

  delete item;
  return 1;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Item_cpp_1name(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  EST_String name = item->name();

  return  env->NewStringUTF(name);
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Item_cpp_1getS(JNIEnv *env, jobject self, jstring jname, jstring jdef)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);
  const char *name = env->GetStringUTFChars(jname, 0);

  EST_feat_status status=efs_ok;

  // cout << "GetS " << name << "\n";
  
  EST_String val= "";

  val = getString(*item, name, EST_String::Empty, status);

  // cout << "<GetS " << name << "=" << val << "\n";
  
  env->ReleaseStringUTFChars(jname, name);

  if (status != efs_ok)
    return jdef;

  // cout << "return\n";
  
  return  env->NewStringUTF(val);
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Item_cpp_1getF(JNIEnv *env, jobject self, jstring jname, jfloat def)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);
  const char *name = env->GetStringUTFChars(jname, 0);

  EST_feat_status status=efs_ok;
  
  float val = getFloat(*item, name, def, status);

  env->ReleaseStringUTFChars(jname, name);
  
  return  val;
}

JNIEXPORT void JNICALL 
Java_cstr_est_Item_cpp_1set__Ljava_lang_String_2F(JNIEnv *env, jobject self, jstring jname, jfloat val)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);
  const char *name = env->GetStringUTFChars(jname, 0);

  item->set(name, val);
  
  env->ReleaseStringUTFChars(jname, name);
}

JNIEXPORT void JNICALL 
Java_cstr_est_Item_cpp_1set__Ljava_lang_String_2Ljava_lang_String_2(JNIEnv *env, jobject self, jstring jname, jstring jval)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);
  const char *name = env->GetStringUTFChars(jname, 0);
  const char *val = env->GetStringUTFChars(jval, 0);

  item->set(name, val);
  
  env->ReleaseStringUTFChars(jname, name);
  env->ReleaseStringUTFChars(jval, val);
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Item_cpp_1getStartTime(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return  item?start(*item):0.0;
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Item_cpp_1getMidTime(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return  item?mid(*item):0.0;
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Item_cpp_1getTime(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return  item?time(*item):0.0;
}

JNIEXPORT jfloat JNICALL 
Java_cstr_est_Item_cpp_1getEndTime(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return  item?end(*item):0.0;
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1getContent(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);
  EST_Item_Content *itemc = item->contents();

  return  (long)itemc;
}

JNIEXPORT jstring JNICALL 
Java_cstr_est_Item_cpp_1type(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  const char *type;

  if (parent(item) != NULL || daughter1(item) != NULL)
    type = "tree";
  else
    type = "linear";

  return  env->NewStringUTF(type);
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1next(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->next();
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1prev(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->prev();
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1up(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->up();
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1down(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->down();
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1insert_1after(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->insert_after();
}

JNIEXPORT jlong JNICALL 
Java_cstr_est_Item_cpp_1insert_1before(JNIEnv *env, jobject self)
{
  EST_Item *item = (EST_Item *) env->GetLongField(self, handle_field);

  return (long)item->insert_before();
}

