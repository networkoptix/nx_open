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
 /*                   Date: Fri Nov 14 1997                               */
 /* --------------------------------------------------------------------  */
 /* Track file formats known to the speech tools.                         */
 /*                                                                       */
 /*************************************************************************/

#ifndef __EST_TRACKFILE_H__
#define __EST_TRACKFILE_H__

#include "EST_Token.h"
#include "EST_TNamedEnum.h"
#include "EST_String.h"
#include "EST_rw_status.h"
#include "htk.h"
#include "ssff.h"

class EST_TrackFile {

public:

  // We have to use #defines for what should be done with just
  // typedefs because Sun CC thinks you shouldn't be allowed to
  // declare a member function via a typedef.

#define LoadTrackFileArgs const EST_String filename, \
			  EST_Track &tr, float ishift, float startt

#define SaveTrackFileArgs const EST_String filename, EST_Track tr

#define LoadTrack_TokenStreamArgs EST_TokenStream &ts, \
				  EST_Track &tr, float ishift, float startt

#define SaveTrac_TokenStreamArgs FILE *fp, EST_Track tr

  typedef EST_read_status  Load_File(LoadTrackFileArgs);
  typedef EST_write_status Save_File(SaveTrackFileArgs);

  typedef EST_read_status  Load_TokenStream(LoadTrack_TokenStreamArgs);

  typedef EST_write_status Save_TokenStream(SaveTrac_TokenStreamArgs);

  typedef struct Info {
    bool recognise;
    Load_File *load;
    Save_File *save;
    const char *description;
  } Info;

  typedef struct TS_Info {
    bool recognise;
    Load_TokenStream *load;
    Save_TokenStream *save;
    const char *description;
  } TS_Info;

  static EST_write_status save_ascii(SaveTrackFileArgs);
  static EST_read_status load_ascii(LoadTrackFileArgs);

  static EST_write_status save_esps(SaveTrackFileArgs);
  static EST_read_status load_esps(LoadTrackFileArgs);

  static EST_write_status save_est_ts(SaveTrac_TokenStreamArgs);
  static EST_read_status load_est_ts(LoadTrack_TokenStreamArgs);

  static EST_write_status save_est(SaveTrackFileArgs);
  static EST_read_status load_est(LoadTrackFileArgs);

  static EST_write_status save_est_binary(SaveTrackFileArgs);
  static EST_write_status save_est_binary_ts(SaveTrac_TokenStreamArgs);

  static EST_write_status save_est_ascii(SaveTrackFileArgs);
  static EST_read_status load_est_ascii(LoadTrackFileArgs);

  static EST_write_status save_htk(SaveTrackFileArgs);
  static EST_read_status load_htk(LoadTrackFileArgs);

  static EST_write_status save_htk_fbank(SaveTrackFileArgs);
  static EST_read_status load_htk_fbank(LoadTrackFileArgs);

  static EST_write_status save_htk_mfcc_e(SaveTrackFileArgs);

  static EST_write_status save_htk_mfcc(SaveTrackFileArgs);
  static EST_read_status load_htk_mfcc(LoadTrackFileArgs);

  static EST_write_status save_htk_user(SaveTrackFileArgs);
  static EST_read_status load_htk_user(LoadTrackFileArgs);

  static EST_write_status save_htk_discrete(SaveTrackFileArgs);
  static EST_read_status load_htk_discrete(LoadTrackFileArgs);

  static EST_write_status save_xmg(SaveTrackFileArgs);
  static EST_read_status load_xmg(LoadTrackFileArgs);

  static EST_write_status save_xgraph(SaveTrackFileArgs);
  static EST_read_status load_xgraph(LoadTrackFileArgs);

  static EST_write_status save_ema(SaveTrackFileArgs);
  static EST_read_status load_ema(LoadTrackFileArgs);

  static EST_write_status save_ema_swapped(SaveTrackFileArgs);
  static EST_read_status load_ema_swapped(LoadTrackFileArgs);

  static EST_write_status save_NIST(SaveTrackFileArgs);
  static EST_read_status load_NIST(LoadTrackFileArgs);

  static EST_write_status save_ssff_ts(SaveTrac_TokenStreamArgs);
  static EST_read_status load_ssff_ts(LoadTrack_TokenStreamArgs);

  static EST_write_status save_ssff(SaveTrackFileArgs);
  static EST_read_status load_ssff(LoadTrackFileArgs);

  static EST_TNamedEnumI<EST_TrackFileType, Info> map;
  static EST_TNamedEnumI<EST_TrackFileType, TS_Info> ts_map;

  static EST_String options_supported(void);
  static EST_String options_short(void);
};

int track_to_espsf0(EST_Track &track, EST_Track &fz);
int espsf0_to_track(EST_Track &fz);

int track_to_htk_lpc(EST_Track &track, EST_Track &lpc);

EST_write_status put_esps(const char *filename, const char *style, float *t, float *a, 
     int *v, float fsize, float rate, int num_points);

enum EST_write_status put_track_esps(const char *filename, char **f_names, 
				     float **a, float fsize, float rate, 
				     int order, int num_points,
			 short fixed);

EST_read_status get_esps(const char *filename, const char *style, 
	     float **t, float **a, int **v, float *fsize, int *num_points);

EST_read_status get_track_esps(const char *filename, char ***fields, float
		 ***a, float *fsize, int *num_points, int *num_fields,
			       short *fixed);

#endif
