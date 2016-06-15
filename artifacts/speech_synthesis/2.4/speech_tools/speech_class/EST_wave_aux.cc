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
/*             Author :  Paul Taylor and Alan Black                      */
/*             Date   :  June 1996                                       */
/*-----------------------------------------------------------------------*/
/*             more EST_Wave class methods                               */
/*                                                                       */
/*=======================================================================*/

#include <cstring>
#include "EST_unix.h"
#include <cstdlib>
#include "EST_cutils.h"
#include "EST_string_aux.h"
#include "EST_Wave.h"
#include "EST_wave_utils.h"
#include "EST_wave_aux.h"
#include "EST_io_aux.h"
#include "EST_error.h"

void extract(EST_Wave &sig, EST_Option &al);

/* Allow EST_Wave to be used in an EST_Val */
VAL_REGISTER_CLASS(wave,EST_Wave)

static EST_TValuedEnumDefinition<EST_sample_type_t, const char *,NO_INFO> st_names[] = {
{st_unknown,  {"undef"}},
{st_schar, {"schar","byte"}},
{st_uchar, {"uchar"}},
{st_short, {"short"}},
{st_shorten, {"shorten"}},
{st_int, {"int"}},
{st_float, {"float"}},
{st_double, {"double"}},
{st_mulaw, {"mulaw"}},
{st_adpcm, {"adpcm"}},
{st_alaw, {"alaw"}},
{st_ascii, {"ascii"}},
{st_unknown, {0}}
};

EST_TNamedEnum<EST_sample_type_t> EST_sample_type_map(st_names);

void differentiate(EST_Wave &sig)
{
    for (int i = 0; i < sig.num_samples() -1; ++i)
	sig.a(i) = sig.a(i + 1) - sig.a(i);
    sig.resize(sig.num_samples()-1);
}


void extract_channels(EST_Wave &single, const EST_Wave &multi,  
		      EST_IList &ch_list)
{

    if (&single == &multi)
    {
	// some nasty person has passed us the same wave for output and input.
	EST_Wave tmp;
	extract_channels(tmp, multi, ch_list);
	single.copy(tmp);
	return;
    }

    int channel, i;
    int c = multi.num_channels();
    int num_samples = multi.num_samples();

    short *buf = new short [num_samples];
    EST_Litem *p;

    single.resize(num_samples, ch_list.length());
    single.set_sample_rate(multi.sample_rate());
    single.set_file_type(multi.file_type());

    for (i = 0, p = ch_list.head(); p; p = p->next(), ++i)
    {
	channel = ch_list(p);
    
	if (channel < 0 || channel >= c)
	    EST_error("Can't extract channel %d from %d channel waveform\n", 
		      channel, c);

	multi.copy_channel(channel, buf);
	single.set_channel(i, buf);
    }
}

int wave_extract_channel(EST_Wave &single, const EST_Wave &multi, int channel)
{
    if (&single == &multi)
    {
	// some nasty person has passed us the same wave for output and input.
	EST_Wave tmp;
	int v = wave_extract_channel(tmp, multi, channel);
	if(v==0)
	    single.copy(tmp);
	return v;
    }

    int c = multi.num_channels();
    
    if (channel < 0 || channel >= c)
    {
	cerr << "Can't extract channel " << channel << " from " << 
	    c << " channel waveform\n";
	return -1;
    }

    EST_Wave subwave;

    multi.sub_wave(subwave, 0, EST_ALL, channel, 1);

    single.copy(subwave);

    return 0;
}

void extract_channels(EST_Wave &single, const EST_Wave &multi,  int channel)
{
    EST_IList a;
    a.append(channel);
    extract_channels(single, multi, a);
}

void wave_combine_channels(EST_Wave &s,const EST_Wave &m)
{
    if (&s == &m)
    {
	// some nasty person has passed us the same wave for output and input.
	EST_Wave tmp;
	wave_combine_channels(tmp,m);
	s = tmp;
	return;
    }
    s.resize(m.num_samples(), 1, FALSE);
    s.set_sample_rate(m.sample_rate());
    
    for(int i=0; i<m.num_samples(); i++)
    {
	double sum=0.0;
	for(int j=0; j<m.num_channels(); j++)
	    sum += m.a(i,j);
	s.a(i,0) = (int)(sum/m.num_channels() + 0.5);
    }
}

void add_waves(EST_Wave &s, const EST_Wave &m)
{
    int new_samples = Gof(s.num_samples(), m.num_samples());
    int new_channels = Gof(s.num_channels(), m.num_channels());
    
    s.resize(new_samples, new_channels, 1);
    
    for (int i = 0; i < m.num_samples(); i++)
	for (int j = 0; j < m.num_channels(); j++)
	    s.a(i, j) += m.a(i, j);
}

void invert(EST_Wave &sig)
{
    sig.rescale(-1.0);
}

void reverse(EST_Wave &sig)
{
    int i, n;
    short t;
    n = (int)floor((float)(sig.num_samples())/2.0);
    
    for (i = 0; i < n; ++i)
    {
	t = sig.a_no_check(i);
	sig.a_no_check(i) = sig.a_no_check(sig.num_samples() - 1 -i);
	sig.a_no_check(sig.num_samples() - 1 -i) = t;
    }
}

void extract(EST_Wave &sig, EST_Option &al)
{
    int from, to;
    EST_Wave sub_wave, w2;
    
    if (al.present("-start"))
	from = (int)(sig.sample_rate() * al.fval("-start"));
    else if (al.present("-from"))
	from = al.ival("-from");
    else
	from = 0;
    
    if (al.present("-end"))
	to = (int)(sig.sample_rate() * al.fval("-end"));
    else if (al.present("-to"))
	to = al.ival("-to");
    else
	to = sig.num_samples();
    
    sig.sub_wave(sub_wave, from, to - from);
    w2 = sub_wave;
    sig = w2;
}

void wave_info(EST_Wave &w)
{
    EST_String t;
    cout << "Duration: " << 
	ftoString((float)w.num_samples()/(float)w.sample_rate(),4,1) << endl;
    
    cout << "Sample rate: " << w.sample_rate() << endl;
    cout << "Number of samples: " << w.num_samples() << endl;
    cout << "Number of channels: " << w.num_channels() << endl;
    cout << "Header type: " << w.file_type() << endl;
    cout << "Data type: " << w.sample_type() << endl;
}

static EST_String options_wave_filetypes(void)
{
    // Returns list of currently support wave file types
    // Should be extracted from the list in EST_Wave (but that's
    // not very clear :-(
    
    return "nist, est, esps, snd, riff, aiff, audlab, raw, ascii";
}

EST_String options_subwave(void)
{
    return
	EST_String("")+
	"-start <float>  Extract sub-wave starting at this time, specified in \n"
	"    seconds\n\n"
	"-end <float>  Extract sub-wave ending at this time, specified in \n"
	"    seconds\n\n"
	"-from <int> Extract sub-wave starting at this sample point\n\n"
	"-to <int> Extract sub-wave ending at this sample point\n\n";
}
EST_String options_wave_input(void)
{
    // The standard waveform input options 
    return
	EST_String("")+
	"-itype <string>  Input file type (optional).  If set to raw, this \n"
	"    indicates that the input file does not have a header. While \n"
        "    this can be used to specify file types other than raw, this is \n"
	"    rarely used for other purposes\n"
        "    as the file type of all the existing supported \n"
	"    types can be determined automatically from the \n"
	"    file's header. If the input file is unheadered, \n"
	"    files are assumed to be shorts (16bit).  \n"
	"    Supported types are \n"
	"   "+options_wave_filetypes()+"\n\n"
	"-n <int>  Number of channels in an unheadered input file \n\n"
	"-f <int> Sample rate in Hertz for an unheadered input file \n\n"
	"-ibo <string>  Input byte order in an unheadered input file: \n"
	"    possibliities are: MSB , LSB, native or nonnative. \n"
	"    Suns, HP, SGI Mips, M68000 are MSB (big endian) \n"
	"    Intel, Alpha, DEC Mips, Vax are LSB (little \n"
	"    endian)\n\n"
	"-iswap  Swap bytes. (For use on an unheadered input file)\n\n"
	"-istype <string> Sample type in an unheadered input file:\n"
	"     short, mulaw, byte, ascii\n\n"
	"-c <string>  Select a single channel (starts from 0). \n"
	"    Waveforms can have multiple channels. This option \n"
        "    extracts a single channel for progcessing and \n"
	"    discards the rest. \n\n"+
	options_subwave();

// old option
//	"-ulaw            Assume unheadered input is 8k ulaw\n\n"
// this facility moved into na_play
//	"-r*              Select subrange of file. (ESPS compatible)\n\n"

}

EST_String options_wave_output(void)
{
    return 
	EST_String("")+
	"-o <ofile>       Output filename. If not specified output is\n"
	"    to stdout.\n\n"
	"-otype <string>  Output file type, (optional).  If no type is\n"
	"    Specified the type of the input file is assumed.\n"
	"    Supported types are: \n"
        "   "+options_wave_filetypes()+"\n\n"
	"-F <int>         Output sample rate in Hz. If this is different \n"
	"    from the input sample rate, resampling will occur \n\n"
	"-obo <string>   Output byte order: MSB, LSB, native, or nonnative. \n"
	"    Suns, HP, SGI Mips, M68000 are MSB (big endian) \n"
	"    Intel, Alpha, DEC Mips, Vax are LSB \n"
	"    (little endian)\n\n"
	"-oswap Swap bytes when saving to output\n\n"+
	"-ostype <string> Output sample type: short, mulaw, byte or ascii\n\n";
}

Declare_TNamedEnum(EST_sample_type_t)
#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TNamedEnum.cc"
Instantiate_TNamedEnum(EST_sample_type_t)
#endif

