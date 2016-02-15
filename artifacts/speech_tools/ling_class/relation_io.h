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
/*                      Author :  Paul Taylor (updated by awb)           */
/*                      Date   :  February 1999                          */
/*-----------------------------------------------------------------------*/
/*                      Relation i/o labels etc                          */
/*                                                                       */
/*=======================================================================*/
#ifndef __RELATION_IO_H__
#define __RELATION_IO_H__

#include "EST_String.h"
#include "EST_Token.h"
#include "EST_rw_status.h"
#include "ling_class/EST_Relation.h"

EST_read_status load_esps_label(EST_TokenStream &ts,EST_Relation &rel);
EST_read_status load_ogi_label(EST_TokenStream &ts, EST_Relation &s);
EST_read_status load_words_label(EST_TokenStream &ts, EST_Relation &s);
EST_read_status load_sample_label(EST_TokenStream &ts,
				   EST_Relation &s, int sample=0);

EST_write_status save_esps_label(const EST_String &filename, 
				 const EST_Relation &s,
				 bool evaluate_ff);
EST_write_status save_htk_label(const EST_String &filename, 
				const EST_Relation &a);

EST_write_status save_esps_label(ostream *outf,
				 const EST_Relation &s,
				 bool evaluate_ff);
EST_write_status save_htk_label(ostream *outf,
				const EST_Relation &a);

#endif /* __RELATION_IO.H__ */
