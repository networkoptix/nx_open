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
/*                  Authors:  Paul Taylor and Simon King                 */
/*                  Date   :  June 1995                                  */
/*-----------------------------------------------------------------------*/
/*                  Label conversion main file                           */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST_ling_class.h"
#include "EST_Track.h"
#include "EST_cmd_line.h"
#include "EST_string_aux.h"

int check_vocab(EST_Relation &a, EST_StrList &vocab);

/** @name <command>ch_lab</command> <emphasis>Label file manipulation</emphasis>
    @id ch_lab_manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
ch_lab is used to manipulate the format of label files and 
serves as a wrap-around for the EST_Relation class.

*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}

int main(int argc, char *argv[])
{
    EST_String out_file, ext;
    EST_StrList files;
    EST_Option al, op;
    EST_Relation lab, key;
    EST_RelationList mlf;
    EST_Litem *p;

    parse_command_line
	(argc, argv, 
	 EST_String(" [input file1] [input file2] -o [output file]\n") +
	 "Summary: change/copy label files\n"+
	 "use \"-\" to make input and output files stdin/out\n"+
	 "-h               Options help\n"+
	 "-S <float>       frame spacing of output\n"+
	 "-base            use base filenames for lists of label files\n"+
	 "-class <string>  Name of class defined in op file\n"+
	 "-combine                  \n"+
	 "-divide                  \n"+
	 "-end <float>     end time (secs) for label extraction\n"+
	 "-ext <string>    filename extension\n"+
	 "-extend <float>  extend track file beyond label file\n"+
	 "-extract <string> extract a single file from a list of label files\n"+
	 "-f <int>         sample frequency of label file\n"+
	 "-itype <string>  type of input label file: esps htk ogi\n"+
	 "-key <string>    key label file\n"+
	 "-lablist <string> list of labels to be considered as blank\n"+
	 "-length <float>  length of track produced\n"+
	 "-lf <int>        sample frequency for labels\n"+
	 "-map <string>    name of file containing label mapping\n"+
	 "-name <string>   eg. Fo Phoneme\n"+
	 "-nopath          ignore pathnames when searching label lists\n"+
	 "-o <ofile>       output gile name\n"+
	 "-off <float>     vertical offset of track\n"+
	 "-ops             print options\n"+
	 "-otype <string> {esps}\n"+
	 "                 output file type: xmg, ascii, esps, htk\n"+
	 "-pad <string>    Pad with \"high\" or \"low\" values\n"+
	 "-pos <string>    list of labels to be regarded as 'pos'\n"+
	 "-q <float>       quantize label timings to nearest value\n"+
	 "-range <float>   different between high and low values\n"+
	 "-sed <ifile>     perform regex editing using sed file\n"+
	 "-shift <float>   shift the times of the labels\n"+
	 "-start <float>   start time for label extraction\n"+
	 "-style <string>  output stype e.g. track\n"+
	 "-vocab <ifile>   file containing list of words in vocab\n"+
	 "-verify          check that only labels in vocab file are in label file\n",
			files, al);

    init_lib_ops(al, op);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    read_RelationList(mlf, files, al);

    // perform all utility functions on all relations in mlf
    for (p = mlf.head(); p; p = p->next())
	relation_convert(mlf(p), al, op);
    
    if (al.present("-verify"))
    {
	EST_StrList vocab;
	if (load_StrList(al.val("-vocab"), vocab) != format_ok)
	{
	    cerr << "Couldn't read vocab file " << al.val("-vocab") 
		<< " for verification\n";
	    exit(-1);
	}
	for (p = mlf.head(); p; p = p->next())
	    check_vocab(mlf(p), vocab);
	exit(0);
    }
    
    if (files.length() == 1)	// special case of only one input file
	lab = mlf.first();

    if (al.present("-extract")) // extract a single relation
	lab = RelationList_extract(mlf, al.val("-extract"), 
				  (bool)al.present("-base"));

    if (al.present("-combine")) // join all relations into lab sequentially
    {
	if (al.present("-key"))
	{
	    key.load(al.val("-key"));
	    lab = RelationList_combine(mlf, key);
	}
	else
	    lab = RelationList_combine(mlf);
    }

    if (al.present("-divide")) // make mlf from single relation and keylab
    {
	EST_StrList blank;
	ext = al.present("-ext") ? al.val("-ext") : (EST_String)"";
	key.load(al.val("-key"));
	if (al.present("-lablist"))
	    StringtoStrList(al.val("-lablist"), blank);
        if (relation_divide(mlf, lab, key, blank, ext) == -1)
	    exit(-1);
//	if (al.present("-newkey")) // the function reassigns keylab boundaries
//	    key.save(al.val("-newkey"));
    }
    
    if (al.val("-style", 0) == "track")
    {
	EST_Track tr;
	label_to_track(lab, al, op, tr);
	tr.save(out_file, op.val("track_file_type", 0));
	exit(0);
    }

    int path = al.present("-nopath") ? 0 : 1;
    if (al.val("-otype", 0) == "mlf")
	save_RelationList(out_file, mlf, 1, path); // i.e. print times
    else if (al.val("-otype", 0) == "wmlf")
	save_RelationList(out_file, mlf, 0, path); // i.e. don't print times
    else if (al.val("-otype", 0) == "words")
	save_WordList(out_file, mlf, 0); 
    else if (al.val("-otype", 0) == "sentence")
	save_WordList(out_file, mlf, 1); 
    else if (al.val("-otype", 0) == "ind")
    {
	if (al.present("-a"))
	    save_ind_RelationList(out_file, mlf, "Addresses", path);
	else
	    save_ind_RelationList(out_file, mlf, "None", path);
    }
    else
//	lab.save(out_file, al.val("-otype", 0), "None");
	lab.save(out_file,al.val("-otype"));

    return 0;
}

void override_lib_ops(EST_Option &a_list, EST_Option &al)
{
    a_list.override_val("frame_shift", al.val("-S", 0));
    a_list.override_val("in_lab_file_type", al.val("-itype", 0));
    a_list.override_val("out_lab_file_type", al.val("-otype", 0));
    a_list.override_val("label_offset", al.val("-off", 0));
    a_list.override_val("label_range", al.val("-range", 0));
    
    if (al.val("-style", 0) == "track")
	a_list.override_val("track_file_type", al.val("-otype", 0));
}

