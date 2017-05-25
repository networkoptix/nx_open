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
/*                       Author :  Paul Taylor and Simon King            */
/*                       Date   :  June 1995                             */
/*-----------------------------------------------------------------------*/
/*                  Stream class auxiliary routines                      */
/*                                                                       */
/*=======================================================================*/

#include <iostream>
#include <fstream>
#include <cmath>
#include "EST_types.h"
#include "EST_FMatrix.h"
#include "ling_class/EST_Relation.h"
#include "EST_Token.h"
#include "EST_string_aux.h"
#include "ling_class/EST_relation_aux.h"
#include "ling_class/EST_relation_compare.h"
#include "EST_io_aux.h"


int close_enough(EST_Item &a, EST_Item &b)
{
    return ((start(&b) < a.F("end")) && (start(&a) < b.F("end")));
}

// WATCH - this uses what should be private access to Keyval class.
void monotonic_match(EST_II_KVL &a, EST_II_KVL &b)
{
    EST_Litem *ptr;

    for (ptr = a.list.head(); ptr != 0; ptr= ptr->next())
    {
	if (a.val(ptr) == -1)
	    continue;
	if (b.val(a.val(ptr)) == a.key(ptr))
//	    cout << "ok\n";
	    continue;
	else
	    a.change_key(ptr, -1);
    }
    for (ptr = b.list.head(); ptr != 0; ptr= ptr->next())
    {
	if (b.val(ptr) == -1)
	    continue;
	if (a.val(b.val(ptr)) == b.key(ptr))
//	    cout << "ok\n";
	    continue;
	else
	    a.change_key(ptr, -1);
    }
}

void function_match(EST_II_KVL &u, EST_Relation &a, EST_Relation &b)
{
    (void)u;
    (void)a;
    (void)b;
#if 0
    EST_Item *a_ptr;
    EST_Litem *i_ptr;
    int i;

    for (i = 0, a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next(), ++i)
    {
	if (a_ptr->f("pos")==1)
	{
	    u.add_item(a_ptr->addr(), -1);
	    for (i_ptr = a_ptr->link(b.stream_name())->head(); i_ptr != 0; 
		 i_ptr = i_ptr->next())
		u.change_val(a_ptr->addr(), a_ptr->link(b.stream_name())->item(i_ptr));
	}
    }
#endif
}

void relation_match(EST_Relation &a, EST_Relation &b)
{
    EST_Item *a_ptr, *b_ptr;

    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if (a_ptr->f("pos")==1)
	    for (b_ptr = b.head(); b_ptr != 0; b_ptr = b_ptr->next())
		{
		    if ((b_ptr->f("pos")==1)
			&&(close_enough(*a_ptr, *b_ptr)))
		    {
//			cout << "linked\n";
#if 0
			link(*a_ptr, *b_ptr);
#endif
		    }
		}

//		if ((b.pos(b_ptr->name()))
//		    &&(close_enough(*a_ptr, *b_ptr)))
//		    link(*a_ptr, *b_ptr);
}

void compare_labels(EST_Relation &reflab, EST_Relation &testlab)
{
    EST_II_KVL uref, utest;

    relation_match(reflab, testlab); // many-to-many mapping

    cout << "Ref\n" << reflab;
    cout << "Test\n" << testlab;

    function_match(uref, reflab, testlab); // one-to-many mapping
    function_match(utest, testlab, reflab); // one-to-many mapping

    cout << "Ref\n" << reflab;
    cout << "Test\n" << testlab;
    cout << "Keyval REF\n" << uref;
    cout << "Keyval TEST\n" << utest;

//    cout << "Keyval REF\n" << uref;
//    cout << "Keyval TEST\n" << utest;

    monotonic_match(uref, utest); // one-to-one mapping

    reassign_links(reflab, uref, testlab.name());
    reassign_links(testlab, utest, reflab.name());
    cout << "Keyval REF\n" << uref;
    cout << "Keyval TEST\n" << utest;

// temporary !!!    

    cout.setf(ios::left,ios::adjustfield);

    cout << "Total: ";
    cout.width(10);
    cout << uref.length();
    cout << "Deletions: ";
    cout.width(10);
    cout << insdel(uref);
    cout << "Insertions: "; 
    cout.width(10);
    cout<< insdel(utest) << endl;
}

EST_Item *nthpos(EST_Relation &a, int n)
{
    EST_Item *a_ptr;
    int i = 0;
    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if (a_ptr->f("pos") == 1)
	{
	    if (n == i)
		return a_ptr;
	    ++i;
	}
    return 0;
}

// measures amount of total overlap between segments
float label_distance1(EST_Item &ref, EST_Item &test)
{
    float s, e;

    s = fabs(start(&ref) - start(&test));
    e = fabs(ref.F("end") - test.F("end"));

    return (s + e) / duration(&ref);
}

// Only penalises a test segment that extends beyond the boundaries of 
// a ref segment
float label_distance2(EST_Item &ref, EST_Item &test)
{
    float s, e;

    s = (start(&test) < start(&ref)) ? start(&ref) - start(&test) : 0;
    e = (ref.F("end") < test.F("end")) ? 
	test.F("end") - ref.F("end") : 0;

    return (s + e) / duration(&ref);
}

int lowest_pos(EST_FMatrix &m, int j)
{
    float val = 1000.0;
    int i, pos=0;

    for (i = 0; i < m.num_rows(); ++i)
	if ((m(i, j) > -0.01) && (m(i, j) < val))
	{
	    val = m(i, j);
	    pos = i;
	}

    return pos;
}

void minimise_matrix_by_column(EST_FMatrix &m)
{
    float val = 1000.0;
    int i;
    for (int j = 0; j < m.num_columns(); ++j)
    {
	val = 1000.0;
	for (i = 0; i < m.num_rows(); ++i)
	    if (m(i, j) < val)
		val = m(i, j);
	for (i = 0; i < m.num_rows(); ++i)
	    if (m(i, j) > val)
		m(i, j) = -1.0;
    }
}

void minimise_matrix_by_row(EST_FMatrix &m)
{
    float val;
    int i, j;

    for (i = 0; i < m.num_rows(); ++i)
    {
	val = 1000.0;
	for (j = 0; j < m.num_columns(); ++j)
	    if ((m(i, j) < val) && (m(i, j) > -0.01))
		val = m(i, j);
	for (j = 0; j < m.num_columns(); ++j)
	    if (m(i, j) > val)
		m(i, j) = -1.0;
    }
}

void matrix_ceiling(EST_FMatrix &m, float max)
{
    int i, j;

    for (i = 0; i < m.num_rows(); ++i)
	for (j = 0; j < m.num_columns(); ++j)
	    if (m(i, j) > max)
		m(i, j) = -1.0;

}

int matrix_insertions(EST_FMatrix &m)
{
    int i, j;
    int n = 0;

    for (i = 0; i < m.num_rows(); ++i)
	for (j = 0; j < m.num_columns(); ++j)
	    if (m(i, j) > -1.0)
		++n;

    return (m.num_rows() - n);
}

int major_matrix_insertions(EST_FMatrix &m, EST_Relation &ref_lab)
{
    int i, j;
    int n = 0;
    EST_Item *s;

    for (i = 0; i < m.num_rows(); ++i)
    {
	s = nthpos(ref_lab, i);
//	cout << s->name() << ": f:" << s->f("minor")<< endl;
	if (s->f("minor") == 1)
	    ++n;
	else
	    for (j = 0; j < m.num_columns(); ++j)
		if (m(i, j) > -1.0)
		    ++n;
    }
    return (m.num_rows() - n);
}

int matrix_deletions(EST_FMatrix &m)
{
    int i, j;
    int n = 0;

    for (j = 0; j < m.num_columns(); ++j)
	for (i = 0; i < m.num_rows(); ++i)
	    if (m(i, j) > -1.0)
		++n;

    return (m.num_columns() - n);
}

int major_matrix_deletions(EST_FMatrix &m, EST_Relation &ref_lab)
{
    int i, j;
    int n = 0;
    EST_Item *s;

    for (j = 0; j < m.num_columns(); ++j)
    {
	s = nthpos(ref_lab, j);
//	cout << s->name() << ": f:" << s->f("minor")<< endl;
	if (s->f("minor") == 1)
	    ++n;
	else
	    for (i = 0; i < m.num_rows(); ++i)
		if (m(i, j) > -1.0)
		    ++n;
    }

    return (m.num_columns() - n);
}

int lowest_pos(float *m, int n)
{
    float val = 1000.0;
    int i, pos=0;

    for (i = 0; i < n; ++i)
	if (m[i] < val)
	{
	    val = m[i];
	    pos = i;
	}

    return pos;
}

void threshold_labels(EST_Relation &reflab, float t)
{
    (void)reflab;
    (void)t;
#if 0
    EST_Item *r_ptr;
    float score=0.0;
    int a;

    for (r_ptr = reflab.head(); r_ptr != 0; r_ptr = r_ptr->next())
	if (r_ptr->f("pos")==1)
	{
	    // REORG - temp comment
//	    score = atof(r_ptr->fields());
	    cout << "score is" << score << endl;

	    a = r_ptr->rlink("blank").first();
	    cout << "score is" << score << " address: " << a << endl;
	    if (score > t)
		cout << "delete\n";
	}
#endif
}

/* Check through relations of each ref label, and make there aren't
multiple ref labels related to the same test label. If this is
discovered, compare scores of all the relevant labels and delete the
relations of all but the lowest.

At this stage, each ref label should have one and only one relation to the
test labels.
*/

void multiple_labels(EST_Relation &reflab)
{
    (void)reflab;
#if 0
    EST_Item *r_ptr, *s_ptr, *t_ptr;;
    EST_Litem *p;
    int a, pos, i;
    EST_TList<int> la;
    float *score;
    score = new float[reflab.length()];

    for (r_ptr = reflab.head(); r_ptr != 0; r_ptr = r_ptr->next())
	if (r_ptr->f("pos")==1)
	{
	    la.clear(); // clear list and add address of current ref label
	    la.append(r_ptr->addr());
	    a = r_ptr->rlink("test").first();
	    cout << a << endl; 

	    // check remainer of ref labels and add any that have same
	    // relations address as r_ptr.
	    for (s_ptr = r_ptr->next(); s_ptr != 0; s_ptr = s_ptr->next())
		if (s_ptr->f("pos")==1)
		    if (s_ptr->rlink("test").first() == a)
			la.append(s_ptr->addr());

	    cout << "la: " << la;
	    if (la.length() > 1) // true if the are multiple relations
	    {
		// find scores of all relevant labels
		for (i = 0, p = la.head(); p!= 0; p = p->next(), ++i)
		{
		    t_ptr = reflab.item(la.item(p));

		    // REORG - temp comment
//		    score[i] = atof(reflab.item(la.item(p))->fields());
		}
		pos  = lowest_pos(score, i); // find position of lowest score

		cout << "best is " << pos << endl;
		// delete relations of all but lowest score
		for (i = 0, p = la.head(); p!= 0; p = p->next(), ++i)
		    if (i != pos)
		    {
			t_ptr = reflab.item(la.item(p));
			t_ptr->rlink("test").clear();
		    }
	    }
	}
#endif

}

/* Compare 2 sets of labels by matrix method. This involves making a M
(number of test labels) x N (number of ref labels) matrix, and
calculating the distance from each label in the test set to each label
in the reference set. The lowest score for each reference is then
recorded. A test is carried out to make sure that no two reference
labels point to the same test label. Then any ref label above a
certain distance is classified as incorrect. The numbers of insertions
and deletions are then calculated.  */

EST_FMatrix matrix_compare(EST_Relation &reflab, EST_Relation &testlab, int method,
		       float t, int v)
{
    int i, j, pos;
    int num_ref, num_test;
    EST_Item *r_ptr, *t_ptr;
    EST_String fns;
    (void)v;

    num_ref = num_test = 0;

    // calculate size of matrix, based on *significant* labels
    for (r_ptr = testlab.head(); r_ptr != 0; r_ptr = r_ptr->next())
	if (r_ptr->f("pos")==1)
	    ++num_test;

    for (r_ptr = reflab.head(); r_ptr != 0; r_ptr = r_ptr->next())
	if (r_ptr->f("pos")==1)
	    ++num_ref;

    EST_FMatrix m(num_test, num_ref);

    if ((m.num_rows() == 0) || (m.num_columns() == 0))
	return m; // nothing to analyse, hence empty matrix 

    // fill matrix values by comparing each test with each reference
    // reference is columns, test is rows

    for (i = 0, t_ptr = testlab.head(); t_ptr != 0; t_ptr = t_ptr->next())
	if (t_ptr->f("pos")==1)
	{
	    for (j = 0, r_ptr = reflab.head(); r_ptr != 0; r_ptr = r_ptr->next())
		if (r_ptr->f("pos")==1)
		{
		    if (method == 1)
			m(i, j) = label_distance1(*r_ptr, *t_ptr);
		    else if (method == 2)
			m(i, j) = label_distance2(*r_ptr, *t_ptr);
		    else
			cerr << "Unknown comparision method" << method << endl;
		    ++j;
		}
	    ++i;
	}

//    cout << "orig M\n";
//    print_matrix_scores(reflab, testlab, m);
    minimise_matrix_by_column(m);
    minimise_matrix_by_row(m);
    matrix_ceiling(m, t);

    // for each ref label, find closest matching test label.
    for (j = 0, r_ptr = reflab.head(); r_ptr != 0; r_ptr = r_ptr->next())
    {
	if (r_ptr->f("pos")==1)
	{
	    pos = lowest_pos(m, j);
	    // REORG - temp comment
//	    r_ptr->set_field_names(r_ptr->fields() +ftoString(m(pos, j)));
	    ++j;
	}
    }
    return m;
}

void multiple_matrix_compare(EST_RelationList &rlist, EST_RelationList
			     &tlist, EST_FMatrix &m, EST_String rpos,
			     EST_String tpos, int method, float t, int v)
{
    EST_Litem *pr, *pt;
    EST_String filename;
    EST_Relation reflab, testlab;
    EST_StrList rposlist, tposlist, rminorlist, tminorlist;
    float ra, rc, mra, mrc;

    StringtoStrList(rpos, rposlist);
    StringtoStrList(tpos, tposlist);
    StringtoStrList("m l mrb mfb lrb lfb", rminorlist);
    StringtoStrList("m l mrb mfb lrb lfb", tminorlist);

    int tot, del, ins, ltot, ldel, lins, lmdel, mdel, lmins, mins;
    tot = del = ins = mdel = mins = 0;

    for (pt = tlist.head(); pt; pt = pt->next())
    {
	pr = RelationList_ptr_extract(rlist, tlist(pt).name(), TRUE);
	if (pr != 0)
	{
	    reflab = rlist(pr);
	    testlab = tlist(pt);

/*	    convert_to_broad(reflab, rposlist);
	    convert_to_broad(testlab, tposlist);
	    convert_to_broad(reflab, rminorlist, "minor");
	    convert_to_broad(testlab, tminorlist, "minor");
*/

//	    cout << "ref\n" << reflab;
//	    cout << "test\n" << testlab;

//	    cout << "features\n";
//	    print_stream_features(reflab);
	    
	    m = matrix_compare(reflab, testlab, method, t, v);
	    
	    ltot = m.num_columns();
	    ldel = matrix_deletions(m);
	    lmdel = major_matrix_deletions(m, reflab);
	    lins = matrix_insertions(m);
	    lmins = major_matrix_insertions(m, testlab);

	    print_results(reflab, testlab, m, ltot, ldel, lins, v);
//	    cout << "Major Deletions: " << lmdel << endl << endl;;

	    tot += ltot;
	    del += ldel;
	    mdel += lmdel;
	    ins += lins;
	    mins += lmins;
	}
    }
    
    rc = float(tot - del)/(float)tot * 100.0;
    ra = float(tot - del -ins)/(float)tot *100.0;
    mrc = float(tot - mdel)/(float)tot * 100.0;
    mra = float(tot - mdel - mins)/(float)tot *100.0;
    
    if (v)
    {
	cout << "Total " << tot << " del: " << del << " ins: " << ins << endl;
	cout << "Total " << tot << " major del " << mdel << " major ins" << mins << endl;
    }
    cout << "Correct " << rc << "%    Accuracy " << ra << "%" << endl;
    cout << "Major Correct " << mrc << "%    Accuracy " << mra << "%" << endl;
}

void error_location(EST_Relation &e, EST_FMatrix &m, int ref)
{
    int i;
    EST_Item *s;

    // reference
    if (ref)
    {
	for (i = 0, s = e.head(); s; s = s->next())
	    if ((int)s->f("pos"))
	    {
		if (column_hit(m, i) >= 0)
		    s->set("hit", 1);
		else
		    s->set("hit", 0);
		++i;
	    }
    }
    else
	for (i = 0, s = e.head(); s; s = s->next())
	    if ((int)s->f("pos"))
	    {
		if (row_hit(m, i) >= 0)
		    s->set("hit", 1);
		else
		    s->set("hit", 0);
		++i;
	    }
}

int insdel(EST_II_KVL &a)
{
    int n = 0;
    EST_Litem *ptr;
    
    for (ptr = a.list.head(); ptr != 0; ptr= ptr->next())
	if (a.val(ptr) == -1)
	    ++n;
    return n;
}

int compare_labels(EST_Utterance &ref, EST_Utterance &test, EST_String name,
		   EST_II_KVL &uref, EST_II_KVL &utest)
{
    // many-to-many mapping
    (void)ref;
    (void)test;
    (void)name;
    (void)uref;
    (void)utest;
#if 0
    relation_match(ref.stream(name), test.stream(name)); 
    
    // one-to-many mapping
    function_match(uref, ref.stream(name), test.stream(name));
    function_match(utest, test.stream(name), ref.stream(name)); 
    
    monotonic_match(uref, utest); // one-to-one mapping
    
    // temporary !!!    
    //    reassign_links(ref.stream(name), uref, name);
    //    reassign_links(test.stream(name), utest, name);
    
    //    cout << "Keyval REF\n" << uref;
    //    cout << "Keyval TEST\n" << utest;
#endif    
    return 0;
}

void reassign_links(EST_Relation &a, EST_Relation &b, EST_II_KVL &ua, EST_II_KVL &ub)
{
    (void)a;
    (void)b;
    (void)ua;
    (void)ub;

#if 0    
    EST_Item *a_ptr, *b_ptr;

    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
    {
	a_ptr->link(b.stream_name())->clear();
	if ((a_ptr->f("pos")==1) && (ua.val(a_ptr->addr()) != -1))
	    a_ptr->make_link(b.stream_name(), ua.val(a_ptr->addr()));
    }
    for (b_ptr = b.head(); b_ptr != 0; b_ptr = b_ptr->next())
    {
	b_ptr->link(a.stream_name())->clear();
	if ((b_ptr->f("pos")==1) && (ub.val(b_ptr->addr()) != -1))
	    b_ptr->make_link(a.stream_name(), ub.val(b_ptr->addr()));
    }
#endif
}

void reassign_links(EST_Relation &a, EST_II_KVL &u, EST_String stream_type)
{
    (void)a;
    (void)u;
    (void)stream_type;
#if 0    
    EST_Item *a_ptr;

    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
    {
	a_ptr->link(stream_type)->clear();
	if ((a_ptr->f("pos")==1) && (u.val(a_ptr->addr()) != -1))
	    a_ptr->make_link(stream_type, u.val(a_ptr->addr()));
    }
#endif
}


int commutate(EST_Item *a_ptr, EST_II_KVL &f1, EST_II_KVL &f2,
	      EST_II_KVL &lref, EST_II_KVL &ltest) 

{ 
    int b, c, d, v;
    (void)lref;			// unused parameter
    (void)a_ptr;
    
//    v = a_ptr->addr();
    v = 0;
    b = f2.val(v);
    c = (b == -1) ? -1: ltest.val(b);
    d = (c == -1) ? -1: f1.val(c);
    
    return d;
    
    //	    c = ltest.val(f2.val(v));
    //	    d = f1.val(ltest.val(f2.val(v)));
}

//    REF		TEST
//		f1
//	S ----------->  S
//	^		^
//   lr |		| lt
//	|	f2	|
//	E ----------->  E
//
//	For a given element in E(ref), "e", if
//	lr(e) == f1(lt(f2(e)))
// 	Then ei has been recognised fully.

void test_labels(EST_Utterance &ref, EST_Utterance &test, EST_Option &op)
{
    (void)ref;
    (void)test;
    (void)op;
#if 0    
    EST_II_KVL f2, inv_f2, inv_f1, f1, lref, ltest;

    compare_labels(ref, test, "Event", f2, inv_f2);
    compare_labels(ref, test, "Syllable", f1, inv_f1);
    
    if (op.present("print_syllable") && op.present("print_map"))
    {
	cout << "Syllable mapping from ref to test\n" << f1;
	cout << "Syllable mapping from test to ref\n" << inv_f1;
    }
    if (op.present("print_event") && op.present("print_map"))
    {
	cout << "Event mapping from ref to test\n" << f2;
	cout << "Event mapping from test to ref\n" << inv_f2;
    }
    
    if (op.present("print_syllable") && op.present("print_ins"))
	cout << "Syllable_insertions: " << insdel(inv_f1) << endl;
    if (op.present("print_syllable") && op.present("print_del"))
	cout << "Syllable_deletions: " << insdel(f1) << endl;
    
    if (op.present("print_event") && op.present("print_ins"))
	cout << "Event_insertions: " << insdel(inv_f2) << endl;
    if (op.present("print_event")  && op.present("print_del"))
	cout << "Event_deletions: " << insdel(f2) << endl;
    
    //    cout << "Ref\n" <<  ref.stream("Event") << ref.stream("Syllable");
    //    cout << "Test\n" << test.stream("Event") << test.stream("Syllable");

    function_match(lref, ref.stream("Event"), ref.stream("Syllable"));
    function_match(ltest, test.stream("Event"), test.stream("Syllable"));
    
    if (op.present("print_functions"))
    {
	cout << "Lref\n" << lref;
	cout << "Ltest\n" << ltest;
	cout << "f1\n" << f1;
	cout << "f2\n" << f2;
    }
    
    EST_Item *a_ptr;
    int correct, n_ev, n_syl;
    
    correct = n_ev = n_syl = 0;
    for (a_ptr = ref.stream("Event").head(); a_ptr != 0; a_ptr = a_ptr->next())
	if (a_ptr->f("pos")==1)
	{
	    ++n_ev;
	    if (lref.val(a_ptr->addr())
		== commutate(a_ptr, f1, f2, lref, ltest))
		++correct;
	}
    for (a_ptr = ref.stream("Syllable").head();a_ptr != 0; a_ptr = a_ptr->next())
	if (a_ptr->f("pos")==1)
	    ++n_syl;
    
    if (op.present("print_syllable") && op.present("print_total"))
	cout << "Number_of_Syllables: " << n_syl << endl;
    if (op.present("print_event") && op.present("print_total"))
	cout << "Number_of_Events: " << n_ev << endl;
    
    if (op.present("print_link"))
	cout << "Correct_links: " << correct <<endl;
    
    if (op.present("print_derivation"))
    {
	for (a_ptr = ref.stream("Event").head();a_ptr!= 0; a_ptr = a_ptr->next())
	{
	    if (a_ptr->f("pos")==1)
	    {
		cout << "Lr(ei): " << lref.val(a_ptr->addr()) << endl;
		cout << "f2(ei): " << f2.val(a_ptr->addr()) << endl;
		cout << "Lt(f2(ei)): " << ltest.val(f2.val(a_ptr->addr()))
		    << endl;
		cout << "f1(Lt(f2(ei))): " 
		    << f1.val(ltest.val(f2.val(a_ptr->addr()))) << endl;
	    }
	    cout << "Event " << *a_ptr;
	    if ( lref.val(a_ptr->addr())
		== f1.val(ltest.val(f2.val(a_ptr->addr()))))
		cout  << " is correct\n";
	    else
		cout  << " is incorrect\n";
	}
    }
#endif
}

void print_i_d_scores(EST_FMatrix &m)
{
    cout.setf(ios::left,ios::adjustfield);
    cout << "Total: ";
    cout.width(10);
    cout << m.num_columns();
    cout << "Deletions: ";
    cout.width(10);
    cout << matrix_deletions(m);
    cout << "Insertions: "; 
    cout.width(10);
    cout<< matrix_insertions(m) << endl;
}

void print_matrix_scores(EST_Relation &ref, EST_Relation &test, EST_FMatrix &a)
{
    int i, j;
    EST_Item *r_ptr, *t_ptr;
    
    cout << "      ";
    for (r_ptr = ref.head(); r_ptr != 0; r_ptr = r_ptr->next())
    {	
	if (r_ptr->f("pos")==1)
	{
	    //	cout.width(5);
	    //	cout.setf(ios::right);
	    cout << r_ptr->name() << " ";
	    cout.width(6);
	    cout.setf(ios::right);
	    cout<< r_ptr->F("end") << " ";
	}
    }
    cout << endl;
    
    for (t_ptr = test.head(), i = 0; i < a.num_rows(); t_ptr = t_ptr->next())
    {
	if (t_ptr->f("pos")==1)
	{
	    cout << t_ptr->name() << " ";
	    for (j = 0; j < a.num_columns(); ++j)
	    {
		cout.width(10);
		cout.precision(3);
		cout.setf(ios::right);
		cout.setf(ios::fixed, ios::floatfield);
		cout << a(i, j) << " ";
	    }
	    cout << endl;
	    ++i;
	}
    }
}

int row_hit(EST_FMatrix &m, int r)
{
    int i;
    for (i = 0; i < m.num_columns(); ++i)
	if (m(r, i) > 0.0)
	    return i;
    
    return -1;
}

// return the row index of the first positive entry in column c
int column_hit(EST_FMatrix &m, int c)
{
    int i;
    for (i = 0; i < m.num_rows(); ++i)
	if (m(i, c) > 0.0)
	    return i;
    
    return -1;
}

int num_b_insertions(EST_FMatrix &m, int last, int current)
{
    int c1, c2;
    c1 = column_hit(m, last);
    c2 = column_hit(m, current);
    
    return c2 - c1 -1;
}

int num_b_deletions(EST_FMatrix &m, int last, int current)
{
    int c1, c2;
    c1 = row_hit(m, last);
    c2 = row_hit(m, current);
    
    return c2 - c1 -1;
}

void print_s_trans(EST_Relation &a, int width)
{
    (void)a;
    (void)width;
//    for (int i = 0; i < a.length(); ++i)
//    {
	//	cout <<	(int)a.nth(i)->f("pos") << " XX " << a.nth(i)->name() << endl;
/*	if ((a.nth(i)->f("pos")==1) || (a.nth(i)->name() == "   "))
	{
	    //	    cout.setf(ios::fixed, ios::floatfield);
	    cout.width(width);
	    cout << a.nth(i)->name() << " ";
	}
  }
*/
    cout << endl;
}

void make_hit_and_miss(EST_Relation &a)
{
    EST_Item *s;
    
    for (s = a.head(); s; s = s->next())
    {
	if (s->f("pos") == 0)
	    s->set_name(".");
	else if (s->f("hit") == 1)
	    s->set_name("HIT");
	else
	    s->set_name("MISS");
	s->features().clear();
    }
}

void pos_only(EST_Relation &lab)
{
    EST_Item *a, *n;
    
    for (a = lab.head(); a; a = n)
    {
	n = a->next();
	if (!a->f_present("pos"))
	    lab.remove_item(a);
    }
}

// Warning this is bugged - slight misalignments occur.
void print_aligned_trans(EST_Relation &ref, EST_Relation &test, EST_FMatrix &m)
{
    (void)ref;
    (void)test;
    (void)m;
/*    int i, j, n;
    EST_Relation al, refal, testal;
    EST_Item *p;
    EST_Item pos, blank;
    
    blank.set_name("   ");
    
    pos.f.set("pos", 1);
    blank.f.set("pos", 0);
    
    pos_only(test);
    pos_only(ref);

    // first check for empty matrices - indicates all insertions or deletions

    if ((m.num_columns() == 0) && (m.num_rows() != 0))
    {
	cout << "REC: ";
	print_s_trans(test);
	return;
    }
    else if ((m.num_columns() != 0) && (m.num_rows() == 0))
    {
	cout << "LAB: ";
	print_s_trans(ref);
	return;
    }
    else if ((m.num_columns() == 0) && (m.num_rows() == 0))
    {
	cout << "LAB: ";
	print_s_trans(ref);
	return;
    }

    int l;
    l = 0;
    
    //    cout << "ref: " << ref.name() << endl << ref;
    //    cout << "test: " << test.name() << endl << test;
    
    if (m(0, 0) < 0)
	refal.append(blank);
    
    
    pos.set_name(ref.head()->name());
    refal.append(pos);	
    for (i = 1, p = ref.head()->next(); p; p = p->next(), ++i)
    {
	n = num_b_insertions(m, l, i);
	
	for (j = 0; j < n; ++j)
	    refal.append(blank);
	
	if (n > -0.5)
	    l = i;
	pos.set_name(p->name());
	
	refal.append(pos);
    }
    
    l = 0;
    pos.set_name(test.head()->name());
    testal.append(pos);
    for (i = 1, p = test.head()->next(); p; p = p->next(), ++i)
    {
	n = num_b_deletions(m, l, i);
	
	//	cout << *p << "last " << l << " current " << i <<
	//	    " insertions " << n << endl;
	for (j = 0; j < n; ++j)
	    testal.append(blank);
	
	if (n > -0.5)
	    l = i;
	pos.set_name(p->name());
	testal.append(pos);
    }
    
    cout << "LAB: ";
    print_s_trans(refal, 3);
    cout << "REC: ";
    print_s_trans(testal, 3);
*/    
}

void print_results(EST_Relation &ref, EST_Relation &test, 
		   EST_FMatrix &m, int tot,
		   int del, int ins, int v)
{
    (void) tot;
    (void) del;
    (void) ins;
    if (v == 0)
	return;
    
    // v == 1 prints out total insertions etc

    if (v == 2)
    {
	cout << basename(ref.name(), "") << endl;
	print_i_d_scores(m);
	cout << endl;
    }
    else if (v == 3)
    {
	cout << basename(ref.name(), "") << endl;
	print_aligned_trans(ref, test, m);
	print_i_d_scores(m);
	cout << endl;
    }
    else if (v == 4)
    {
	cout << basename(ref.name(), "") << endl;
	print_matrix_scores(ref, test, m);
	print_i_d_scores(m);
	cout << endl;
    }
    else if (v == 5)
    {
	cout << basename(ref.name(), "") << endl;
	print_matrix_scores(ref, test, m);
	print_aligned_trans(ref, test, m);
	print_i_d_scores(m);
	cout << endl;
    }
    else if (v == 6)
    {
	print_matrix_scores(ref, test, m);
	error_location(ref, m, 1);
	make_hit_and_miss(ref);
	error_location(test, m, 0);
	make_hit_and_miss(test);
	ref.save("ref.error");
	test.save("test.errors");
    }
    else if (v == 7)
    {
	error_location(ref, m, 1);
	make_hit_and_miss(ref);
	error_location(test, m, 0);
	make_hit_and_miss(test);
	ref.save("ref.error");
	test.save("test.error");
    }
}
