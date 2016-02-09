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
 /* Auxiliary operations on utterance structures.                         */
 /*                                                                       */
 /*************************************************************************/

#include "ling_class/EST_utterance_aux.h"
#include "EST_UtteranceFile.h"
#include "genxml.h"


EST_String options_utterance_filetypes(void)
{
    // Returns list of currently support utterance filetypes
    // Should be extracted from the list in EST_Utterance
    
    return EST_UtteranceFile::options_short();
}

EST_String options_utterance_filetypes_long(void)
{
    // Returns list of currently support utterance filetypes
    // Should be extracted from the list in EST_Utterance
    
    return EST_UtteranceFile::options_supported();
}

 /*************************************************************************/
 /*                                                                       */
 /* XML related functions. the first two are stubs if RXP is not          */
 /* available. The third is not available at all.                         */
 /*                                                                       */
 /*************************************************************************/

void utterance_xml_register_id(const EST_String pattern, 
			const EST_String result)
{
  (void)pattern; (void)result;
#if defined(INCLUDE_XML_FORMATS)
EST_GenXML::register_id(pattern, 
		   result);
#endif
}

void utterance_xml_registered_ids(EST_StrList &list)
{
  (void)list;
#if defined(INCLUDE_XML_FORMATS)
EST_GenXML::registered_ids(list);
#endif
}


#if defined(INCLUDE_XML_FORMATS)
#include "ling_class/EST_utterance_xml.h"

InputSource utterance_xml_try_and_open(Entity ent)
{
  return EST_GenXML::try_and_open(ent);
}

#endif
