/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1997                            */
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
/*                     Author :  Alan W Black                            */
/*                     Date   :  November 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Some internal classes for Weighted Finite State Transducers           */
/*                                                                       */
/*=======================================================================*/
#ifndef __WFST_AUX_H__
#define __WFST_AUX_H__

// Used in minimization

class wfst_marks {
  private:
    int p_x;
    char **p_mark_table;  // triangular matrix
    char val(int p,int q) 
        { if (p < q) return p_mark_table[q][p];
          else return p_mark_table[p][q]; }
    void set_val(int p,int q,char e) 
        { if (p < q) p_mark_table[q][p] = e;
          else p_mark_table[p][q] = e; }
  public:
    wfst_marks(int x);
    ~wfst_marks();

    int distinguished(int p, int q) { return val(p,q) == 'd'; }
    int undistinguished(int p, int q) 
       { return val(p,q) == 'u'; }
    void distinguish(int p, int q) { set_val(p,q,'d'); }
    void undistinguish(int p, int q) { set_val(p,q,'u'); }
    void find_state_map(EST_IVector &state_map,int &num_new_states);
				      
};

typedef EST_TKVL<int,EST_IList> wfst_assumes;
void mark_undistinguished(wfst_marks &marks,wfst_assumes &assumptions);
int equivalent_to(int y,int z,wfst_assumes &assumptions);
void add_assumption(int y,int z,wfst_assumes &assumptions);

#endif
