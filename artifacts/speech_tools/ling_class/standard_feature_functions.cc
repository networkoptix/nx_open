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
 /*             Originally: Paul Taylor (pault@cstr,ed,ac,uk)             */
 /* --------------------------------------------------------------------  */
 /* Some generally useful feature functions.                              */
 /*                                                                       */
 /* Mostly not very general and way too specific, these shouldn't be here */
 /*                                                                       */
 /*************************************************************************/

#include "ling_class/EST_Item.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_Item_Content.h"
#include "ling_class/EST_item_aux.h"

EST_Val ff_duration(EST_Item *s)
{
    if (s->prev())
        return s->F("end")-s->prev()->F("end");
    else
        return s->F("end");
}

EST_Val ff_start(EST_Item *s)
{
    /* Changed by awb 12/07/05, to make this actually a generic function */
    /* no longer changes relation view to Segment -- may affect tilt and */
    /* other pault things                                                */
    return  (s->prev() == 0) ? 0.0 : s->prev()->F("end");
}

EST_Val ff_tilt_phrase_position(EST_Item *s)
{
    EST_String rel_name = s->S("time_path");
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	{
	    cerr << "item: " << *s << endl;
	    EST_error("No relation %s for item\n", (const char *) rel_name);
	}

    a = parent(t);

    cout << "us features phrase pos\n";
    //cout << "dereferencing syllable: " << *a << endl;
    cout << "start: " << a->F("start") << endl;
    cout << "end: " << a->F("end") << endl;

    if (s->S("name","0") == "phrase_start")
        return a->F("start");
    else
        return a->F("end");
}

EST_Val ff_tilt_event_position(EST_Item *s)
{
    EST_String rel_name = s->S("time_path");
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	EST_error("No relation %s for item\n", (const char *) rel_name);

    a = parent(t);

    cout << "us features tilt pos\n";
    cout << "dereferencing syllable: " << *a << endl;
    cout << "vowel_start: " << a->F("vowel_start") << endl;
    cout << "start: " << a->F("start") << endl;
    cout << "end: " << a->F("end") << endl;
    
    return a->F("vowel_start") + s->F("rel_pos",0.0);
}

EST_Val ff_leaf_end(EST_Item *s)
{
    if (!s->f_present("time_path"))
	EST_error("Attempted to use leaf end() feature function on "
		  "item with no time_path feature set: %s\n", 
		  (const char *)s->relation()->name());

    EST_String rel_name = s->S("time_path");
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	EST_error("No relation %s for item\n", (const char *) rel_name);

    a = last_leaf_in_tree(t);

    float def = -1.0;
    EST_feat_status stat;
    return getFloat(*a, "end", def, stat);
}

EST_Val ff_leaf_start(EST_Item *s)
{
    if (!s->f_present("time_path"))
	EST_error("Attempted to use leaf start() feature function on "
		  "item with no time_path feature set: %s\n", 
		  (const char *)s->relation()->name());

    EST_String rel_name = s->S("time_path");
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	EST_error("No relation %s for item\n", (const char *) rel_name);

    a = first_leaf_in_tree(t);
    //    cout << "this is the first node of the tree\n";
    //cout << *a << endl;

    float def = -1.0;
    EST_feat_status stat;
    return getFloat(*a, "start", def, stat);
}

EST_Val ff_int_start(EST_Item *s)
{
    EST_String rel_name = "IntonationPhrase";
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	EST_error("No relation %s for item\n", (const char *) rel_name);

    a = first_leaf_in_tree(parent(t)->as_relation("MetricalTree"));

    float def = -1.0;
    EST_feat_status stat;
    return getFloat(*a, "start", def, stat);
}

EST_Val ff_int_end(EST_Item *s)
{
    EST_String rel_name = "IntonationPhrase";
    EST_Item *t, *a;

    if ((t = s->as_relation(rel_name)) == 0)
	EST_error("No relation %s for item\n", (const char *) rel_name);

    a = last_leaf_in_tree(parent(t)->as_relation("MetricalTree"));

    float def = -1.0;
    EST_feat_status stat;
    return getFloat(*a, "end", def, stat);
}

void register_standard_feature_functions(EST_FeatureFunctionPackage &p)
{
#ifdef	EST_DEBUGGING
  cerr << "register_standard_feature_functions()\n";
#endif
    p.register_func("duration", ff_duration);
    p.register_func("start", ff_start);
    p.register_func("leaf_end", ff_leaf_end);
    p.register_func("leaf_start", ff_leaf_start);
    p.register_func("int_end", ff_int_end);
    p.register_func("int_start", ff_int_start);
    p.register_func("tilt_event_position", ff_tilt_event_position);
    p.register_func("tilt_phrase_position", ff_tilt_phrase_position);
    p.register_func("unisyn_duration", ff_duration);
    p.register_func("unisyn_start", ff_start);
    p.register_func("unisyn_leaf_end", ff_leaf_end);
    p.register_func("unisyn_leaf_start", ff_leaf_start);
    p.register_func("unisyn_int_end", ff_int_end);
    p.register_func("unisyn_int_start", ff_int_start);
    p.register_func("unisyn_tilt_event_position", ff_tilt_event_position);
    p.register_func("unisyn_tilt_phrase_position", ff_tilt_phrase_position);
#ifdef	EST_DEBUGGING
  cerr << "finished register_standard_feature_functions()\n";
#endif
}



