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
 /* --------------------------------------------------------------------  */
 /* Wave file formats known to the speech tools.                         */
 /*                                                                       */
 /*************************************************************************/

#ifndef __EST_WAVEFILE_H__
#define __EST_WAVEFILE_H__

#include "EST_TNamedEnum.h"
#include "EST_String.h"
#include "EST_Token.h"
#include "EST_rw_status.h"
#include "EST_Wave.h"
#include "EST_wave_aux.h"

typedef enum EST_WaveFileType{
  wff_none,
  wff_nist,
  wff_esps,
  wff_est,
  wff_audlab,
  wff_snd,
  wff_aiff,
  wff_riff,
  wff_raw,
  wff_ulaw
} EST_WaveFileType;

class EST_WaveFile {
public:

  // We have to use #defines for what should be done with just
  // typedefs because Sun CC thinks you shouldn't be allowed to
  // declare a member function via a typedef.

#define LoadWaveFileArgs const EST_String filename, \
				EST_Wave &wv, \
				int rate, EST_sample_type_t stype, \
				int bo, int nc, \
				int offset, int length

#define SaveWaveFileArgs const EST_String filename, \
				const EST_Wave &wv, \
				EST_sample_type_t stype, int bo

#define LoadWave_TokenStreamArgs EST_TokenStream &ts, \
				EST_Wave &wv, \
				int rate, EST_sample_type_t stype, \
				int bo, int nc, \
				int offset, int length

#define SaveWave_TokenStreamArgs FILE *fp, \
				    const EST_Wave &wv, \
				    EST_sample_type_t stype, int bo

  typedef EST_read_status  Load_TokenStream(LoadWave_TokenStreamArgs);

  typedef EST_write_status Save_TokenStream(SaveWave_TokenStreamArgs);

  typedef struct Info {
    bool recognise;
    Load_TokenStream *load;
    Save_TokenStream *save;
    const char *description;
  } Info;

  static EST_write_status save_nist(SaveWave_TokenStreamArgs);
  static EST_read_status load_nist(LoadWave_TokenStreamArgs);

  static EST_write_status save_est(SaveWave_TokenStreamArgs);
  static EST_read_status load_est(LoadWave_TokenStreamArgs);

  static EST_write_status save_esps(SaveWave_TokenStreamArgs);
  static EST_read_status load_esps(LoadWave_TokenStreamArgs);

  static EST_write_status save_audlab(SaveWave_TokenStreamArgs);
  static EST_read_status load_audlab(LoadWave_TokenStreamArgs);

  static EST_write_status save_snd(SaveWave_TokenStreamArgs);
  static EST_read_status load_snd(LoadWave_TokenStreamArgs);

  static EST_write_status save_aiff(SaveWave_TokenStreamArgs);
  static EST_read_status load_aiff(LoadWave_TokenStreamArgs);

  static EST_write_status save_riff(SaveWave_TokenStreamArgs);
  static EST_read_status load_riff(LoadWave_TokenStreamArgs);

  static EST_write_status save_raw(SaveWave_TokenStreamArgs);
  static EST_read_status load_raw(LoadWave_TokenStreamArgs);

  static EST_write_status save_ulaw(SaveWave_TokenStreamArgs);
  static EST_read_status load_ulaw(LoadWave_TokenStreamArgs);

  static EST_TNamedEnumI<EST_WaveFileType, Info> map;

  static EST_String options_supported(void);
  static EST_String options_short(void);
};

#endif
