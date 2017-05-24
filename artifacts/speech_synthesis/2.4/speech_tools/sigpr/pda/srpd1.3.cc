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
/*                      Author :  Paul Bagshaw                           */
/*                      Date   :  1993                                   */
/*************************************************************************/
/*                                                                       */
/* The above copyright was given by Paul Bagshaw, he retains             */
/* his original rights                                                   */
/*                                                                       */
/*************************************************************************/
 /****************************************************************************
 *                                                                           *
 * Pitch Determination Algorithm.                                            *
 *                                                                           *
 * Super Resolution Pitch Determinator with No Headers (SRPD_HD).            *
 *                                                                           *
 * Analysis synchronised with cepstral analysis, pitch biasing option, and   *
 * optimised for minimum gross pitch errors and accurate voiced/unvoiced     *
 * classification. All known bugs resolved!                                  *
 *                                                                           *
 * 4th February 1992:                                                        *
 * Additional option [-w] added to give an artificial frame length, thus     *
 * allowing the output data to be synchronised with other signal processing  *
 * algorithms such as cepstral analysis and formant tracking.                *
 *                                                                           *
 * Y. Medan, E. Yair, and D. Chazan, "Super resolution pitch determination   *
 * of speech signals," IEEE Trans. Signal Processing Vol.39 No.1             *
 * pp.40-48 (1991).                                                          *
 *                                                                           *
 * Implementation by Paul Bagshaw, Centre for Speech Technology Research,    *
 * University of Edinburgh, 80 South Bridge, Edinburgh EH1 1HN.              *
 *                                                                           *
 *****************************************************************************/

/************************
 * include header files *
 ************************/

#include <cmath>
#include <cstdlib>
#include <iostream>
#include "srpd.h"
#include "EST_cutils.h"
#include "EST_Wave.h"

#ifndef MAXSHORT
#define MAXSHORT 32767
#endif

void super_resolution_pda (struct Srpd_Op *paras, SEGMENT_ seg,
			   CROSS_CORR_ *p_cc, STATUS_ *p_status)
{

  static int zx_lft_N, zx_rht_N;
  static double prev_pf = BREAK_NUMBER;

  int n, j, k, N0 = 0, N1, N2, N_, q, lower_found = 0, score = 1, apply_bias;
  int x_index, y_index, z_index;
  int zx_rate = 0, zx_at_N0 = 0, prev_sign;
  int seg1_zxs = 0, seg2_zxs = 0, total_zxs;
  short prev_seg1, prev_seg2;
  short x_max = -MAXSHORT, x_min = MAXSHORT;
  short y_max = -MAXSHORT, y_min = MAXSHORT;
  double xx = 0.0, yy = 0.0, zz = 0.0, xy = 0.0, yz = 0.0, xz = 0.0;
  double max_cc = 0.0, coefficient, coeff_weight;
  double xx_N, yy_N, xy_N, y1y1_N, xy1_N, yy1_N, beta;
  LIST_ *sig_pks_hd, *sig_pks_tl, *sig_peak, *head, *tail;
  
  sig_pks_hd = head = NULL;
  sig_pks_tl = tail = NULL;
  /* set correlation coefficient threshold */
  if (p_status->v_uv == UNVOICED || p_status->v_uv == SILENT)
    p_status->threshold = paras->Thigh;
  else /* p_status->v_uv == VOICED */
    p_status->threshold = (paras->Tmin > paras->Tmax_ratio *
        p_status->cc_max) ? paras->Tmin : paras->Tmax_ratio *
	p_status->cc_max;
  /* determine if a bias should be applied */
  if (paras->peak_tracking && prev_pf != BREAK_NUMBER &&
      p_status->v_uv == VOICED && p_status->s_h != HOLD &&
      p_status->pitch_freq < 1.75 * prev_pf &&
      p_status->pitch_freq > 0.625 * prev_pf)
    apply_bias = 1;
  else
    apply_bias = 0;
  /* consider first two segments of period n = Nmin */
  prev_seg1 = seg.data[paras->Nmax - paras->Nmin] < 0 ? -1 : 1;
  prev_seg2 = seg.data[paras->Nmax] < 0 ? -1 : 1;
  for (j = 0; j < paras->Nmin; j += paras->L) {
    /* find max and min amplitudes in x and y segments */
    x_index = paras->Nmax - paras->Nmin + j;
    y_index = paras->Nmax + j;
    if (seg.data[x_index] > x_max) x_max = seg.data[x_index];
    if (seg.data[x_index] < x_min) x_min = seg.data[x_index];
    if (seg.data[y_index] > y_max) y_max = seg.data[y_index];
    if (seg.data[y_index] < y_min) y_min = seg.data[y_index];
    /* does new sample in x or y segment represent an input zero-crossing */
    if (seg.data[x_index] * prev_seg1 < 0) {
      prev_seg1 *= -1;
      seg1_zxs++;
    }
    if (seg.data[y_index] * prev_seg2 < 0) {
      prev_seg2 *= -1;
      seg2_zxs++;
    }
    /* calculate parts for first correlation coefficient */
    xx += (double) seg.data[x_index] * seg.data[x_index];
    yy += (double) seg.data[y_index] * seg.data[y_index];
    xy += (double) seg.data[x_index] * seg.data[y_index];
  }
  /* low amplitude segment represents silence */
  if (abs (x_max) + abs (x_min) < 2 * paras->Tsilent || 
      abs (y_max) + abs (y_min) < 2 * paras->Tsilent) {
    for (q = 0; q < p_cc->size; p_cc->coeff[q++] = 0.0);
    prev_pf = p_status->pitch_freq;
    p_status->pitch_freq = BREAK_NUMBER;
    p_status->v_uv = SILENT;
    p_status->s_h = SEND;
    p_status->cc_max = 0.0;
    return;
  }
  /* determine first correlation coefficients, for period n = Nmin */
  p_cc->coeff[0] = p_status->cc_max = xy / sqrt (xx) / sqrt (yy);
  for (q = 1; q < p_cc->size && q < paras->L; p_cc->coeff[q++] = 0.0);
  total_zxs = seg1_zxs + seg2_zxs;
  prev_sign = p_cc->coeff[0] < 0.0 ? -1 : 1;
  prev_seg1 = seg.data[paras->Nmax - paras->Nmin] < 0 ? -1 : 1;
  /* iteratively determine correlation coefficient for next possible period */
  for (n = paras->Nmin + paras->L; n <= paras->Nmax; n += paras->L,
       j += paras->L) {
    x_index = paras->Nmax - n;
    y_index = paras->Nmax + j;
    /* does new samples in x or y segment represent an input zero-crossing */
    if (seg.data[x_index] * prev_seg1 < 0) {
      prev_seg1 *= -1;
      total_zxs++;
    }
    if (seg.data[y_index] * prev_seg2 < 0) {
      prev_seg2 *= -1;
      total_zxs++;
    }
    /* determine next coefficient */
    xx += (double) seg.data[x_index] * seg.data[x_index];
    yy += (double) seg.data[y_index] * seg.data[y_index];
    for (k = 0, xy = 0.0; k < n; k += paras->L)
      xy += (double) seg.data[paras->Nmax - n + k] * seg.data[paras->Nmax + k];
    p_cc->coeff[n - paras->Nmin] = xy / sqrt (xx) / sqrt (yy);
    if (p_cc->coeff[n - paras->Nmin] > p_status->cc_max)
      p_status->cc_max = p_cc->coeff[n - paras->Nmin];
    /* set unknown coefficients to zero */
    for (q = n - paras->Nmin + 1;
	 q < p_cc->size && q < n - paras->Nmin + paras->L;
	 p_cc->coeff[q++] = 0.0);
    /* is there a slope with positive gradient in the coefficients track yet */
    if (p_cc->coeff[n - paras->Nmin] > p_cc->coeff[n - paras->Nmin - paras->L])
      lower_found = 1;
    /* has new coefficient resulted in a zero-crossing */
    if (p_cc->coeff[n - paras->Nmin] * prev_sign < 0.0) {
      prev_sign *= -1;
      zx_rate++;
    }
    /* does the new coefficient represent a pitch period candidate */
    if (N0 != 0 && zx_rate > zx_at_N0) {
      add_to_list (&sig_pks_hd, &sig_pks_tl, N0, 1);
      N0 = 0;
      max_cc = 0.0;
    }
    if (apply_bias && n > zx_lft_N && n < zx_rht_N)
      coeff_weight = 2.0;
    else
      coeff_weight = 1.0;
    if (p_cc->coeff[n - paras->Nmin] > max_cc && total_zxs > 3 && lower_found) {
      max_cc = p_cc->coeff[n - paras->Nmin];
      if (max_cc * coeff_weight >= p_status->threshold) {
	zx_at_N0 = zx_rate;
	N0 = n;
      }
    }
  }
  /* unvoiced if no significant peak found in coefficients track */
  if (sig_pks_hd == NULL) {
    prev_pf = p_status->pitch_freq;
    p_status->pitch_freq = BREAK_NUMBER;
    p_status->v_uv = UNVOICED;
    p_status->s_h = SEND;
    return;
  }
  /* find which significant peak in list corresponds to true pitch period */
  sig_peak = sig_pks_hd;
  while (sig_peak != NULL) {
    yy = zz = yz = 0.0;
    for (j = 0; j < sig_peak->N0; j++) {
      y_index = paras->Nmax + j;
      z_index = paras->Nmax + sig_peak->N0 + j;
      yy += (double) seg.data[y_index] * seg.data[y_index];
      zz += (double) seg.data[z_index] * seg.data[z_index];
      yz += (double) seg.data[y_index] * seg.data[z_index];
    }
    if (yy == 0.0 || zz == 0.0)
      coefficient = 0.0;
    else
      coefficient = yz / sqrt (yy) / sqrt (zz);
    if (apply_bias && sig_peak->N0 > zx_lft_N && sig_peak->N0 < zx_rht_N)
      coeff_weight = 2.0;
    else
      coeff_weight = 1.0;
    if (coefficient * coeff_weight >= p_status->threshold) {
      sig_peak->score = 2;
      if (head == NULL) {
	head = sig_peak;
	score = 2;
      }
      tail = sig_peak;
    }
    sig_peak = sig_peak->next_item;
  }
  if (head == NULL) head = sig_pks_hd;
  if (tail == NULL) tail = sig_pks_tl;
  N0 = head->N0;
  if (tail != head) {
    xx = 0.0;
    for (j = 0; j < tail->N0; j++)
      xx += (double) seg.data[paras->Nmax - tail->N0 + j] *
	  seg.data[paras->Nmax - tail->N0 + j];
    sig_peak = head;
    while (sig_peak != NULL) {
      if (sig_peak->score == score) {
	xz = zz = 0.0;
	for (j = 0; j < tail->N0; j++) {
	  z_index = paras->Nmax + sig_peak->N0 + j;
	  xz += (double) seg.data[paras->Nmax - tail->N0 + j] *
	      seg.data[z_index];
	  zz += (double) seg.data[z_index] * seg.data[z_index];
	}
	coefficient = xz / sqrt (xx) / sqrt (zz);
	if (sig_peak == head)
	  max_cc = coefficient;
	else if (coefficient * paras->Tdh > max_cc) {
	  N0 = sig_peak->N0;
	  max_cc = coefficient;
	}
      }
      sig_peak = sig_peak->next_item;
    }
  }
  p_status->cc_max = p_cc->coeff[N0 - paras->Nmin];
  /* voiced segment period now found */
  if ((tail == head && score == 1 && p_status->v_uv != VOICED) ||
      p_cc->coeff[N0 - paras->Nmin] < p_status->threshold)
    p_status->s_h = HOLD;
  else
    p_status->s_h = SEND;
  /* find left and right boundaries of peak in coefficients track */
  zx_lft_N = zx_rht_N = 0;
  for (q = N0; q >= paras->Nmin; q -= paras->L)
    if (p_cc->coeff[q - paras->Nmin] < 0.0) {
      zx_lft_N = q;
      break;
    }
  for (q = N0; q <= paras->Nmax; q += paras->L)
    if (p_cc->coeff[q - paras->Nmin] < 0.0) {
      zx_rht_N = q;
      break;
    }
  /* define small region around peak */
  if (N0 - paras->L < paras->Nmin) {
    N1 = N0;
    N2 = N0 + 2 * paras->L;
  }
  else if (N0 + paras->L > paras->Nmax) {
    N1 = N0 - 2 * paras->L;
    N2 = N0;
  }
  else {
    N1 = N0 - paras->L;
    N2 = N0 + paras->L;
  }
  /* compensate for decimation factor L */
  if (paras->L != 1) {
    xx = yy = xy = 0.0;
    for (j = 0; j < N1; j++) {
      x_index = paras->Nmax - N1 + j;
      y_index = paras->Nmax + j;
      xx += (double) seg.data[x_index] * seg.data[x_index];
      xy += (double) seg.data[x_index] * seg.data[y_index];
      yy += (double) seg.data[y_index] * seg.data[y_index];
    }
    p_cc->coeff[N1 - paras->Nmin] = p_status->cc_max =
        xy / sqrt (xx) / sqrt (yy);
    N0 = N1;
    for (n = N1 + 1; n <= N2; n++, j++) {
      xx += (double) seg.data[paras->Nmax - n] * seg.data[paras->Nmax - n];
      yy += (double) seg.data[paras->Nmax + j] * seg.data[paras->Nmax + j];
      for (k = 0, xy = 0.0; k < n; k++)
	xy += (double) seg.data[paras->Nmax - n + k] * seg.data[paras->Nmax + k];
      p_cc->coeff[n - paras->Nmin] = xy / sqrt (xx) / sqrt (yy);
      if (p_cc->coeff[n - paras->Nmin] > p_status->cc_max) {
	p_status->cc_max = p_cc->coeff[n - paras->Nmin];
	N0 = n;
      }
    }
  }
  /* compensate for finite resolution in estimating pitch */
  if (N0 - 1 < paras->Nmin || N0 == N1) N_ = N0;
  else if (N0 + 1 > paras->Nmax || N0 == N2) N_ = N0 - 1;
  else if (p_cc->coeff[N0 - paras->Nmin] - p_cc->coeff[N0 - paras->Nmin - 1] <
	   p_cc->coeff[N0 - paras->Nmin] - p_cc->coeff[N0 - paras->Nmin + 1])
    N_ = N0 - 1;
  else
    N_ = N0;
  xx_N = yy_N = xy_N = y1y1_N = xy1_N = yy1_N = 0.0;
  for (j = 0; j < N_; j++) {
    x_index = paras->Nmax - N_ + j;
    y_index = paras->Nmax + j;
    xx_N += (double) seg.data[x_index] * seg.data[x_index];
    yy_N += (double) seg.data[y_index] * seg.data[y_index];
    xy_N += (double) seg.data[x_index] * seg.data[y_index];
    y1y1_N += (double) seg.data[y_index + 1] * seg.data[y_index + 1];
    xy1_N += (double) seg.data[x_index] * seg.data[y_index + 1];
    yy1_N += (double) seg.data[y_index] * seg.data[y_index + 1];
  }
  beta = (xy1_N * yy_N - xy_N * yy1_N) /
      (xy1_N * (yy_N - yy1_N) + xy_N * (y1y1_N - yy1_N));
  if (beta < 0.0) {
    N_--;
    beta = 0.0;
  }
  else if (beta >= 1.0) {
    N_++;
    beta = 0.0;
  }
  else
    p_status->cc_max = ((1.0 - beta) * xy_N + beta * xy1_N) /
      sqrt (xx_N * ((1.0 - beta) * (1.0 - beta) * yy_N +
		    2.0 * beta * (1.0 - beta) * yy1_N +
		    beta * beta * y1y1_N));
  prev_pf = p_status->pitch_freq;
  p_status->pitch_freq = (double) (paras->sample_freq) / (double) (N_ + beta);
  p_status->v_uv = VOICED;
  free_list (&sig_pks_hd);
  return;

}

/************* * LEVEL TWO * ************/

void add_to_list (LIST_ **p_list_hd, LIST_ **p_list_tl, int N_val, 
		  int score_val)
{

  LIST_ *new_node, *last_node;

  new_node = walloc(LIST_ ,1);
  last_node = *p_list_tl;
  new_node->N0 = N_val;
  new_node->score = score_val;
  new_node->next_item = NULL;
  if (*p_list_hd == NULL)
    *p_list_hd = new_node;
  else
    last_node->next_item = new_node;
  *p_list_tl = new_node;

}

/********************
 * define functions *
 ********************/

/************* * LEVEL ONE * ************/

void error (error_flags err_type)
{

  char prog[15]; /* program file name */

  strcpy (prog, "srpd");
  fprintf (stderr, "%s: ", prog);
  switch (err_type) {
  case CANT_WRITE:
    fprintf (stderr, "cannot write to output file");
    break;
  case DECI_FCTR:
    fprintf (stderr, "decimation factor not set");
    break;
  case INSUF_MEM:
    fprintf (stderr, "insufficient memory available");
    break;
  case FILE_ERR:
    perror ("");
    break;
  case FILE_SEEK:
    fprintf (stderr, "improper fseek () to reposition a stream");
    break;
  case LEN_OOR:
    fprintf (stderr, "artificial frame length set out of range");
    break;
  case MAX_FREQ:
    fprintf (stderr, "maximum pitch frequency value (Hz) not set");
    break;
  case MIN_FREQ:
    fprintf (stderr, "minimum pitch frequency value (Hz) not set");
    break;
  case MISUSE:
    fprintf (stderr, "usage: %s -i lpf_sample_file ", prog);
    fprintf (stderr, "-o pitch_file [options]\n");
    fprintf (stderr, "\nOptions {with default values}\n");
    fprintf (stderr, "-a form pitch_file in ascii format\n");
    fprintf (stderr, "-l 'lower pitch frequency limit' {%f (Hz)}\n",
	     DEFAULT_MIN_PITCH);
    fprintf (stderr, "-u 'upper pitch frequency limit' {%f (Hz)}\n",
	     DEFAULT_MAX_PITCH);
    fprintf (stderr, "-d 'decimation factor' {%d (samples)}\n",
	     DEFAULT_DECIMATION);
    fprintf (stderr, "-n 'noise floor (abs. amplitude)' {%d}\n",
	     DEFAULT_TSILENT);
    fprintf (stderr, "-h 'unvoiced to voiced coeff threshold' {%f}\n",
	     DEFAULT_THIGH);
    fprintf (stderr, "-m 'min. voiced to unvoiced coeff threshold' {%f}\n",
	     DEFAULT_TMIN);
    fprintf (stderr, "-r 'voiced to unvoiced coeff threshold ratio' {%f}\n",
	     DEFAULT_TMAX_RATIO);
    fprintf (stderr, "-t 'anti pitch doubling/halving threshold' {%f}\n",
	     DEFAULT_TDH);
    fprintf (stderr, "-p perform peak tracking\n");
    fprintf (stderr, "-f 'sampling frequency' {%d (Hz)}\n", DEFAULT_SF);
    fprintf (stderr, "-s 'frame shift' {%f (ms)}\n", DEFAULT_SHIFT);
    fprintf (stderr, "-w 'artificial frame length' {%f (ms)}\n",
	     DEFAULT_LENGTH);
    break;
  case NOISE_FLOOR:
    fprintf (stderr, "noise floor set below minimum amplitude");
    break;
  case SAMPLE_FREQ:
    fprintf (stderr, "attempt to set sampling frequency negative");
    break;
  case SFT_OOR:
    fprintf (stderr, "frame shift set out of range");
    break;
  case THR_DH:
    fprintf (stderr, "anti pitch doubling/halving threshold not set");
    break;
  case THR_HIGH:
    fprintf (stderr, "unvoiced to voiced coeff threshold not set");
    break;
  case THR_MAX_RTO:
    fprintf (stderr, "voiced to unvoiced coeff threshold ratio not set");
    break;
  case THR_MIN:
    fprintf (stderr, "minimum voiced to unvoiced coeff threshold not set");
    break;
  default:
    fprintf (stderr, "undefined error, %u occurred", err_type);
    break;
  }
  fprintf (stderr, "\n");
  exit (-1);

}

void initialise_parameters (struct Srpd_Op *p_par)
{
  p_par->L = DEFAULT_DECIMATION;
  p_par->min_pitch = DEFAULT_MIN_PITCH;
  p_par->max_pitch = DEFAULT_MAX_PITCH;
  p_par->shift = DEFAULT_SHIFT;
  p_par->length = DEFAULT_LENGTH;
  p_par->Tsilent = DEFAULT_TSILENT;
  p_par->Tmin = DEFAULT_TMIN;
  p_par->Tmax_ratio = DEFAULT_TMAX_RATIO;
  p_par->Thigh = DEFAULT_THIGH;
  p_par->Tdh = DEFAULT_TDH;
  p_par->make_ascii = 0;
  p_par->peak_tracking = 0;
  p_par->sample_freq = DEFAULT_SF;
  /* p_par->Nmax and p_par->Nmin cannot be initialised */
  return;

}

void initialise_structures (struct Srpd_Op *p_par, SEGMENT_ *p_seg,
     CROSS_CORR_ *p_cc)
{
  p_par->Nmax = (int) ceil((float)p_par->sample_freq / p_par->min_pitch);
  p_par->Nmin = (int) floor((float)p_par->sample_freq / p_par->max_pitch);
  p_par->min_pitch = (float)p_par->sample_freq / (float)p_par->Nmax;
  p_par->max_pitch = (float)p_par->sample_freq / (float)p_par->Nmin;

  p_seg->size = 3 * p_par->Nmax;
  p_seg->shift = (int) rint( p_par->shift / 1000.0 * (float)p_par->sample_freq );
  p_seg->length = (int) rint( p_par->length / 1000.0 * (float)p_par->sample_freq );
  p_seg->data = walloc(short,p_seg->size);

  p_cc->size = p_par->Nmax - p_par->Nmin + 1;
  p_cc->coeff = walloc(double,p_cc->size);

  return;
}


void initialise_status (struct Srpd_Op *paras, STATUS_ *p_status)
{

  p_status->pitch_freq = BREAK_NUMBER;
  p_status->v_uv = SILENT;
  p_status->s_h = SEND; /* SENT */
  p_status->cc_max = 0.0;
  p_status->threshold = paras->Thigh;
  return;

}

void end_structure_use(SEGMENT_ *p_seg, CROSS_CORR_ *p_cc)
{

  wfree (p_seg->data);
  wfree (p_cc->coeff);
  return;

}

#define BEGINNING 1
#define MIDDLE_   2
#define END       3

int read_next_segment (FILE *voxfile, struct Srpd_Op *paras, SEGMENT_ *p_seg)
{

  static int status = BEGINNING, padding= -1, tracklen = 0;

  int samples_read = 0;
  long init_file_position, offset;

  if (status == BEGINNING) {
    if (padding == -1) {
      if (fseek (voxfile, 0L, 2)) error (FILE_SEEK);
      tracklen = ((ftell (voxfile) / sizeof (short)) - p_seg->length) /
				 p_seg->shift + 1;
	cout << "track len " << tracklen;
      rewind (voxfile);
      if (paras->Nmax < p_seg->length / 2) {
	offset = (long) (p_seg->length / 2 - paras->Nmax) * sizeof (short);
	if (fseek (voxfile, offset, 1)) error (FILE_SEEK);
	padding = 0;
      }
      else {
	if ((paras->Nmax - p_seg->length / 2) % p_seg->shift != 0) {
	  offset = (long) (p_seg->shift - ((paras->Nmax - p_seg->length / 2) %
					   p_seg->shift)) * sizeof (short);
	  if (fseek (voxfile, offset, 1)) error (FILE_SEEK);
	}
	padding = (paras->Nmax - p_seg->length / 2) / p_seg->shift +
	  ((paras->Nmax - p_seg->length / 2) % p_seg->shift == 0 ? 0 : 1);
   }
  }
    cout << "padding " << padding << endl;
    if (padding-- == 0)
      status = MIDDLE_;
    else if (tracklen-- <= 0)
      return (0);
    else
      return (2);
  }
  cout << "tl  " << tracklen << endl;
  if (status == MIDDLE_) {
    if (tracklen > 0) {
      init_file_position = ftell (voxfile);
      offset = (long) (p_seg->shift * sizeof (short));
      samples_read = fread ((short *) p_seg->data, sizeof (short),
			    p_seg->size, voxfile);
      if (samples_read == p_seg->size) {
	if (fseek (voxfile, init_file_position + offset, 0)) error (FILE_SEEK);
	tracklen--;
	return (1);
      }
      else {
	status = END;
      }
    }
    else
      return (0);
  }
  if (status == END) {
    if (tracklen-- > 0)
      return (2);
    else
      return (0);
  }
  return (0);

}

int read_next_wave_segment(EST_Wave &sig, Srpd_Op *paras, SEGMENT_ *p_seg)
{
    static int status = BEGINNING, padding = -1, tracklen = 0;
    int i;
    long offset;
    static int wave_pos;
    

    //printf("read:  size %d shift %d length %d\n", p_seg->size, p_seg->shift, p_seg->length);

    if (status == BEGINNING) 
    {
	if (padding == -1) 
	{
	    tracklen = (sig.num_samples() - p_seg->length) 
		/ p_seg->shift + 1;
	    if (paras->Nmax < p_seg->length / 2) 
	    {
		offset = p_seg->length / 2 - paras->Nmax;
		wave_pos = offset;
		padding = 0;
	    }
	    else 
	    {
		if ((paras->Nmax - p_seg->length / 2) % p_seg->shift != 0) {
		    offset = p_seg->shift - ((paras->Nmax - p_seg->length / 2)%
						     p_seg->shift);
		    wave_pos = offset;
		}
		padding = (paras->Nmax - p_seg->length / 2) / p_seg->shift +
		    ((paras->Nmax - p_seg->length / 2) 
		     % p_seg->shift == 0 ? 0 : 1);
	    }
	}
	if (padding-- == 0)
	    status = MIDDLE_;
	else if (tracklen-- <= 0) {
		status = BEGINNING;
		padding = -1;
		tracklen = 0;
	    return (0);
	}
	else
	    return (2);
    }
    if (status == MIDDLE_)
    {
	if (tracklen > 0) 
	{
	    offset = p_seg->shift;
	    for (i = 0; (i < p_seg->size) && (i+wave_pos)<sig.num_samples();
		 ++i)
		p_seg->data[i] = sig.a(i + wave_pos,0);
	    for ( ; i < p_seg->size; ++i)
		p_seg->data[i] = 0;

	    if (wave_pos <= sig.num_samples())
	    {
		wave_pos += offset;
		tracklen--;
		return (1);
	    }
	    else
		status = END;
	}
	else {
		status = BEGINNING;
		padding = -1;
		tracklen = 0;
	    return (0);
	    }
    }
    if (status == END) 
    {
	if (tracklen-- > 0)
	    return (2);
	else	{
		status = BEGINNING;
		padding = -1;
		tracklen = 0;
	    return (0);
	    }
    }
    status = BEGINNING;
    padding = -1;
    tracklen = 0;
    return (0);
}

void write_track(STATUS_ status, struct Srpd_Op paras, FILE *outfile)
{
  if (paras.make_ascii) 
  {
      if (fprintf(outfile,"%7g\n",status.pitch_freq) != 8)
	  error(CANT_WRITE);
  }
  else
    if (!fwrite ((double *) &status.pitch_freq, sizeof (double), 1, outfile))
      error (CANT_WRITE);
  return;

}

void free_list (LIST_ **p_list_hd)
{

  LIST_ *next;

  while (*p_list_hd != NULL) {
    next = (*p_list_hd)->next_item;
    wfree (*p_list_hd);
    *p_list_hd = next;
  }

}
