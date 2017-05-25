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


#ifndef __EST_UTTERANCEFILE_H__
#define __EST_UTTERANCEFILE_H__

#include "EST_TNamedEnum.h"
#include "ling_class/EST_Utterance.h"
#include "EST_string_aux.h"
#include "EST_FileType.h"
#include "EST_Token.h"

/** Table of different file formats for loading an saving utterances.
  * 
  * @author Richard Caley <rjc@cstr.ed.ac.uk>
  * @version $Id: EST_UtteranceFile.h,v 1.3 2003/01/15 11:13:50 robert Exp $
  */

typedef enum EST_UtteranceFileType{
  uff_none,
  uff_est,
  uff_est_ascii=uff_est,
  uff_xlabel,
  uff_genxml,
  uff_apml,
} EST_UtteranceFileType;



class EST_UtteranceFile {
public:

  // We have to use #defines for what should be done with just
  // typedefs because Sun CC thinks you shouldn't be allowed to
  // declare a member function via a typedef.

#define LoadUtterance_TokenStreamArgs EST_TokenStream &ts, \
				      EST_Utterance &u, \
				      int &max_id

#define SaveUtterance_TokenStreamArgs ostream &outf,const \
				      EST_Utterance &utt

  typedef EST_read_status  Load_TokenStream(LoadUtterance_TokenStreamArgs);

  typedef EST_write_status Save_TokenStream(SaveUtterance_TokenStreamArgs);

  typedef struct Info {
    bool recognise;
    Load_TokenStream *load;
    Save_TokenStream *save;
    const char *description;
  } Info;

  static EST_write_status save_est_ascii(SaveUtterance_TokenStreamArgs);
  static EST_read_status load_est_ascii(LoadUtterance_TokenStreamArgs);

  static EST_write_status save_xlabel(SaveUtterance_TokenStreamArgs);
  static EST_read_status load_xlabel(LoadUtterance_TokenStreamArgs);

  static EST_write_status save_genxml(SaveUtterance_TokenStreamArgs);
  static EST_read_status load_genxml(LoadUtterance_TokenStreamArgs);

  static EST_read_status load_apml(LoadUtterance_TokenStreamArgs);

  static EST_TNamedEnumI<EST_UtteranceFileType, Info> map;

  static EST_String options_supported(void);
  static EST_String options_short(void);
};

#endif

