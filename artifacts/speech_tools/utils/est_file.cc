/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  March 1998                             */
/*-----------------------------------------------------------------------*/
/*                File functions for EST type files                      */
/*                                                                       */
/*=======================================================================*/

#include "EST_FileType.h"
#include "EST_TNamedEnum.h"
#include "EST_Token.h"
#include "EST_Option.h"
#include "EST_Features.h"

static EST_TValuedEnumDefinition<EST_EstFileType, const char *, NO_INFO> 
estfile_names[] =
{
  { est_file_none,	{ "None" }},
  { est_file_track,	{ "Track", "track" }},
  { est_file_wave,	{ "wave" }},
  { est_file_label,	{ "label" }},
  { est_file_utterance,	{ "utterance" }},
  { est_file_fmatrix,	{ "fmatrix" }},
  { est_file_fvector,	{ "fvector" }},
  { est_file_dmatrix,	{ "dmatrix" }},
  { est_file_dvector,	{ "dvector" }},
  { est_file_feature_data, { "feature_data" }},
  { est_file_fst,	{ "fst" }},
  { est_file_ngram,	{ "ngram" }},
  { est_file_index,	{ "index" }},
  { est_file_f_catalogue,	{ "f_catalogue" }},
  { est_file_unknown,	{ "unknown" }},
  { est_file_none,	{ "None" }},
};

EST_TNamedEnum<EST_EstFileType> EstFileEnums(estfile_names);

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TNamedEnum.cc"
template class EST_TNamedEnum<EST_EstFileType>;
template class EST_TNamedEnumI<EST_EstFileType, NO_INFO>;
template class EST_TValuedEnum<EST_EstFileType,const char *>;
template class EST_TValuedEnumI<EST_EstFileType,const char *, NO_INFO>;
#endif

/** Read and parse the header of an EST_File - interim version
returning features rather than EST_Option
*/

EST_read_status read_est_header(EST_TokenStream &ts, EST_Features &hinfo, 
				bool &ascii, EST_EstFileType &t)
{
    EST_String k, v;
    char magic_number[9];
    int pos;

    // read initial file type identifier, can't use peek or get
    // as that could read *way* too far if it's binary so just read
    // the first n bytes to change the magic number
    pos = ts.tell();
    if ((ts.fread(magic_number,sizeof(char),8) != 8) ||
	(strncmp(magic_number,"EST_File",8) != 0))
    {
	ts.seek(pos);
	return wrong_format;
    }

    v = ts.get().string();
    t =  EstFileEnums.token(v);

    if (t == est_file_none)
    {
	// Its not a standardly defined type but did have EST_File on
	// it so accept it but set FileType in the header info
	t = est_file_unknown;
	hinfo.set("FileType", v);
    }

    while ((!ts.eof()) && (ts.peek().string() != "EST_Header_End"))
    { // note this *must* be done using temporary variables
	k = ts.get().string();
	v = ts.get_upto_eoln().string();

	if (v.contains(RXwhite, 0))
	  v = v.after(RXwhite);

	hinfo.set(k, v);
    }

    if (ts.eof())
    {
	cerr << "Unexpected end of EST_File" << endl;
	return misc_read_error;
    }
    ts.get().string();		// read control EST_Header_End

    // If it explicitly says binary it is, otherwise its ascii
    if (hinfo.S("DataType") == "binary")
	ascii = false;
    else
	ascii = true;

    return format_ok;
}

EST_read_status read_est_header(EST_TokenStream &ts, EST_Option &hinfo, 
				bool &ascii, EST_EstFileType &t)
{
    EST_String k, v;
    char magic_number[9];
    int pos;

    // read initial file type identifier, can't use peek or get
    // as that could read *way* too far if it's binary so just read
    // the first n bytes to change the magic number
    pos = ts.tell();
    if ((ts.fread(magic_number,sizeof(char),8) != 8) ||
	(strncmp(magic_number,"EST_File",8) != 0))
    {
	ts.seek(pos);
	return wrong_format;
    }

    v = ts.get().string();
    t =  EstFileEnums.token(v);

    if (t == est_file_none)
    {
	// Its not a standardly defined type but did have EST_File on
	// it so accept it but set FileType in the header info
	t = est_file_unknown;
	hinfo.add_item("FileType",v);
    }

    while ((!ts.eof()) && (ts.peek().string() != "EST_Header_End"))
    { // note this *must* be done using temporary variables
	k = ts.get().string();
	v = ts.get_upto_eoln().string();

	if (v.contains(RXwhite, 0))
	  v = v.after(RXwhite);

	hinfo.add_item(k, v);
    }

    if (ts.eof())
    {
	cerr << "Unexpected end of EST_File" << endl;
	return misc_read_error;
    }
    ts.get().string();		// read control EST_Header_End

    // If it explicitly says binary it is, otherwise its ascii
    if (hinfo.sval("DataType",0) == "binary")
	ascii = false;
    else
	ascii = true;

    return format_ok;
}
