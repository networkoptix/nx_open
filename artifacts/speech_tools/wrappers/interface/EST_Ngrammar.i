/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
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
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                     Author :  Korin Richmond                          */
/*                     Date   :  20 May 2003                             */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* SWIG interface file defining interface to speech tools ngram code     */
/*                                                                       */
/*=======================================================================*/

%module EST_Ngrammar

%{
#include "EST_Ngrammar.h"
#include "EST_DMatrix.h"
%}

%include "EST_rw_status.i"
%import EST_typemaps.i

class EST_Ngrammar {
public:
  // 3 representations : sparse, dense and backed off.
  enum representation_t {sparse, dense, backoff};
  enum entry_t {frequencies, log_frequencies};
  
public:
  EST_Ngrammar();

  EST_Ngrammar(int o, representation_t r, const EST_StrList &wordlist);
  // When state trans vocab differs from prediction vocab
  EST_Ngrammar(int o, representation_t r, const EST_StrList &wordlist, const EST_StrList &predlist);

  ~EST_Ngrammar();
    
  void clear();
  bool init(int o, representation_t r, const EST_StrList &wordlist);
  bool init(int o, representation_t r, const EST_StrList &wordlist, const EST_StrList &predlist);
    
  // access
  int num_states(void) const;
  double samples(void) const;
  int order() const;
  int get_vocab_length();
  EST_String get_vocab_word(int i) const;
  int get_vocab_word(const EST_String &s) const;
  int get_pred_vocab_length() const;
  EST_String get_pred_vocab_word(int i) const;
  int get_pred_vocab_word(const EST_String &s) const;
  int closed_vocab() const;
  entry_t entry_type() const;
  representation_t representation() const;
    
  // build
  bool build(const EST_StrList &filenames,
	     const EST_String &prev = SENTENCE_START_MARKER,
	     const EST_String &prev_prev = SENTENCE_END_MARKER,
	     const EST_String &last = SENTENCE_END_MARKER,
	     const EST_String &input_format = "",
	     const EST_String &oov_mode = "",
	     const int mincount=1,
	     const int maxcount=10);
    
  // Accumulate ngrams
  void accumulate(const EST_StrVector &words, const double count=1);
  void accumulate(const EST_IVector   &words, const double count=1);
    
  // I/O functions 
  EST_read_status  load(const EST_String &filename);
  EST_read_status  load(const EST_String &filename, const EST_StrList &wordlist);
  EST_write_status save(const EST_String &filename, EST_String type="cstr_ascii", 
			bool trace=false, double floor=0.0);
  
  int wordlist_index(const EST_String &word, bool report=true) const;
  const EST_String &wordlist_index(int i) const;
  int predlist_index(const EST_String &word) const;
  const EST_String &predlist_index(int i) const;
    
  // set
  bool set_entry_type(entry_t new_type);
  bool set_representation(representation_t new_representation);

  // probability distributions
  // -------------------------
  // flag 'force' forces computation of probs on-the-fly if necessary
  double probability(const EST_StrVector &words, bool force=false, bool trace=false) const;
  double frequency(  const EST_StrVector &words, bool force=false, bool trace=false) const;

  const EST_String &predict(const EST_StrVector &words) const;
  const EST_String &predict(const EST_StrVector &words, double *prob) const;
  const EST_String &predict(const EST_StrVector &words, double *prob, int *state) const;
    
  const EST_String &predict(const EST_IVector   &words) const;
  const EST_String &predict(const EST_IVector   &words, double *prob) const;
  const EST_String &predict(const EST_IVector   &words, double *prob, int *state) const;
    
  int find_state_id(const EST_StrVector &words) const;
  int find_state_id(const EST_IVector   &words) const;
  int find_next_state_id(int state, int word) const;

  // reverse - probability of words[0..order-2] given word[order-1]
  double reverse_probability(const EST_StrVector &words, bool force=false) const;
  double reverse_probability(const EST_IVector   &words, bool force=false) const;
    
  // predict, where words has 'order' elements and the last one is "" or NULL
  const EST_DiscreteProbDistribution &prob_dist(const EST_StrVector &words) const;
  const EST_DiscreteProbDistribution &prob_dist(const EST_IVector &words) const;
  const EST_DiscreteProbDistribution &prob_dist(int state) const;
    
  void fill_window_start(EST_IVector &window, 
			 const EST_String &prev, 
			 const EST_String &prev_prev) const;

  void fill_window_start(EST_StrVector &window, 
			 const EST_String &prev,
			 const EST_String &prev_prev) const;
  
  bool ngram_exists(const EST_StrVector &words) const;
  bool ngram_exists(const EST_StrVector &words, const double threshold) const;
  const double get_backoff_weight(const EST_StrVector &words) const;
  bool set_backoff_weight(const EST_StrVector &words, const double w);
  
  void print_freqs(ostream &os,double floor=0.0);

  // frequencies below mincount get backed off
  // frequencies above maxcount are not smoothed(discounted)
  bool compute_backoff_weights(const int mincount=1,
			       const int maxcount=10);
  
  
  bool merge(EST_Ngrammar &n,float weight);
};

// Auxiliary functions  
// smoothing
void frequency_of_frequencies(EST_DVector &ff, EST_Ngrammar &n, int this_order);
void map_frequencies(EST_Ngrammar &n, const EST_DVector &map, int this_order);
bool Good_Turing_smooth(EST_Ngrammar &n, int maxcount, int mincount);
void Good_Turing_discount(EST_Ngrammar &ngrammar, int maxcount, double default_discount);

void fs_build_backoff_ngrams(EST_Ngrammar *backoff_ngrams, EST_Ngrammar &ngram);
int fs_backoff_smooth(EST_Ngrammar *backoff_ngrams, EST_Ngrammar &ngram, int smooth_thresh);

void Ngram_freqsmooth(EST_Ngrammar &ngram, int smooth_thresh1, int smooth_thresh2);

bool test_stats(EST_Ngrammar &ngram, 
		const EST_String &filename,
		double &raw_entropy,
		double &count,
		double &entropy,
		double &perplexity,
		const EST_String &input_format,
		const EST_String &prev = SENTENCE_START_MARKER, 
		const EST_String &prev_prev = SENTENCE_END_MARKER,
		const EST_String &last = SENTENCE_END_MARKER);
