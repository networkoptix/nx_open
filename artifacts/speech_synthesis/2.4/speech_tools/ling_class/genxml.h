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


/**@name genxml.h
  * 
  * Privare header interfacing gen file format.
  * 
  * @author Richard Caley <rjc@cstr.ed.ac.uk>
  * @version $Id: genxml.h,v 1.2 2001/04/04 13:11:27 awb Exp $
  */
//@{
#ifndef __GENXML_H__
#define __GENXML_H__

#include <stdio.h>
#include "ling_class/EST_Utterance.h"
#include "rxp/XML_Parser.h"
#include "EST_types.h"

class EST_GenXML
{
public:
  static EST_read_status read_xml(FILE *file, 
			      const EST_String &name,
			      EST_Utterance &u,
			      int &max_id);

  static void register_id(const EST_String pattern, 
			  const EST_String result);

  static void registered_ids(EST_StrList &list);
  
  static InputSource try_and_open(Entity ent);


private:
  static XML_Parser_Class *pclass;

protected:
  static void class_init(void);
  friend class ling_class_init;
};

#endif
//@}
