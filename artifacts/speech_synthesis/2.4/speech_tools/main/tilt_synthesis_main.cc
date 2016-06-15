/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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
/*                    Author :  Paul Taylor                              */
/*                    Date   :  February 1996                            */
/*-----------------------------------------------------------------------*/
/*                    Event RFC Synthesis                                */
/*                                                                       */
/*=======================================================================*/

#include "EST_cmd_line.h"
#include "EST_tilt.h"
#include "EST_Track.h"
#include "ling_class/EST_relation_aux.h"
#include "EST_string_aux.h"

/** @name <command>tilt_synthesis</command> <emphasis>Generate F0 contours from Tilt descriptions</emphasis>
 * @id tilt_synthesis-manual
  * @toc
 */

//@{

void extract_channels(EST_Wave &single, const EST_Wave &multi,  EST_IList &ch_list);

/**@name Synopsis
  */
//@{

//@synopsis

/**
tilt_synthesis generates a F0 contour, given a label file containing
parameterised Tilt or RFC events.

A detailed description of the Tilt intonation model can be found in the
<link linkend="tilt-overview">Tilt model overview</link> section.


*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main (int argc, char *argv[])
{
    EST_Track fz, nfz;
    EST_Relation ev;
    EST_Option al, op;
    EST_String out_file("-"), ev_format, pstring;
    EST_StrList files, event_list;
    EST_Item *e;

    float shift;
    const float default_frame_shift = 0.01; // i.e 10ms intervals

    parse_command_line
	(argc, argv,
       EST_String("[input label file] -o [output file] [options]") +
	 "Summary: generate F0 file from tilt or RFC label file\n"
	 "use \"-\" to make input and output files stdin/out\n"
	 "-h               Options help\n\n"+
	 "-noconn          Synthesize events only - no connections in output\n"
	 "-o <ofile>       Output F0 file\n"
	 "-otype <string>  File type for output label file\n"
	 "-event_names   <string>  List of labels to be classed as events. \n"
	 "     Lists are specified as quoted strings with spaces \n"
	 "     separating each item, e.g.: \"a b c d\"\n\n"
	 "-s <float>       Frame spacing of generated contour in seconds\n",
	files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";
    init_lib_ops(al, op);

    ev.load(files.first());

    // temporary fix until status of start and end is finalised
    float prev_end = 0.0;

    for (e = ev.head(); e; e = e->next())
    {
	e->set("start", prev_end);
	prev_end = e->F("end");
    }

    pstring = al.present("-event_names") ? al.val("-event_names"): 
    EST_String("a b ab pos");
    StringtoStrList(pstring, event_list);

    convert_to_broad(ev, event_list, "int_event");
    shift = al.present("-s") ? al.fval("-s") : default_frame_shift;

    if (ev.f("intonation_style") == "tilt")
	tilt_synthesis(fz, ev, shift, al.present("-noconn"));
    else
    {
//	validate_rfc_stream(ev);
	fill_rfc_types(ev);
//	cout << ev;
	rfc_synthesis(fz, ev, shift, al.present("-noconn"));
    }

    fz.set_channel_name("F0", 0);

    fz.save(out_file, al.val("-otype"));
    return 0;
}

/** @name Input Intonation Files

The input should be a label file containing the tilt parameters for the
events in feature format. An example, in xlabel format, is shown below:
</para>
<para>
<screen>
intonation_style tilt
#
0.29 26     phrase_start ; ev.f0 115.234 ; position 0.29 ; 
0.53 26     a ; int_event 1 ; ev.f0 118.171 ; position 0.53 ; tilt.amp 21.8602 ; 
              tilt.dur 0.26 ; tilt.tilt -0.163727 ; 
0.77 26     a ; int_event 1 ; ev.f0 112.694 ; position 0.77 ; tilt.amp 27.0315 ; 
              tilt.dur 0.32 ; tilt.tilt -0.446791 ; 
1.53 26     a ; int_event 1 ; ev.f0 100.83 ; position 1.53 ; tilt.amp 7.507 ; 
              tilt.dur 0.22 ; tilt.tilt -0.296317 ; 
1.79 26     phrase_end ; ev.f0 92.9785 ; position 1.79 ; 
</screen>
</para>
<para>
tilt_synthesis can also generate F0 contours from RFC parameters:
</para>
<para>
<screen>
intonation_style rfc
#
0.29 26     phrase_start ; ev.f0 115.234 ; position 0.29 ; 
0.53 26     a ; ev.f0 118.171 ; rfc.rise_amp 8.19178 ; rfc.rise_dur 0.12 ; 
               rfc.fall_amp -13.6684 ; rfc.fall_dur 0.14 ; position 0.53 ;
 0.77 26     a ; ev.f0 112.694 ; rfc.rise_amp 6.50673 ; rfc.rise_dur 0.1 ;
                rfc.fall_amp -20.5248 ; rfc.fall_dur 0.22 ; position 0.77 ; 
1.53 26     a ; ev.f0 100.83 ; rfc.rise_amp 1.55832 ; rfc.rise_dur 0.11 ; 
                rfc.fall_amp -6.09238 ; rfc.fall_dur 0.11 ; position 1.53 ; 
1.79 26     phrase_end ; ev.f0 92.9785 ; position 1.79 ; 
</screen>
</para>
<para>
The feature in the header, "intonation_style tilt" or
"intonation_style rfc" is needed for the tilt_synthesis program to know which
type of synthesis to perform.

*/

//@{
//@}

//@}


void override_lib_ops(EST_Option &a_list, EST_Option &al)
{
    // general options
    a_list.override_val("sample_rate", al.val("-f", 0));
}

