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
 /************************************************************************/
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)            */
 /*                   Date: Fri May  9 1997                              */
 /************************************************************************/
 /*                                                                      */
 /* Track maps provide a mapping from symbolic track names to the        */
 /* actual position of the information within a track frame.             */
 /*                                                                      */
 /* Channel name maps map textual names for track channels to symbolic   */
 /* names, they are just a special case of named enums.                  */
 /*                                                                      */
 /************************************************************************/

#include "EST_TrackMap.h"

void EST_TrackMap::clear(void)
{
  for(int i=0; i<num_channel_types; i++)
    p_map[i]=NO_SUCH_CHANNEL;
}

void EST_TrackMap::copy(EST_TrackMap &from)
{
  for(int i=0; i<num_channel_types; i++)
    p_map[i]=from.p_map[i];
}


void EST_TrackMap::init(void)
{
clear(); 
p_parent=NULL;
p_offset=0; 
}

EST_TrackMap::EST_TrackMap(void)
{
  init();
}


EST_TrackMap::EST_TrackMap(int refcount)
{
  init();
  if (refcount)
    start_refcounting();
}

EST_TrackMap::EST_TrackMap(const EST_TrackMap *parent, int offset, int refcount)
{
  init();
  p_parent = (EST_TrackMap *)parent;
  p_offset=offset;
  if (refcount)
    start_refcounting();
}

EST_TrackMap::EST_TrackMap(EST_TrackMap &from, int refcount)
{
  copy(from);
  if (refcount)
    start_refcounting();
}

EST_TrackMap::EST_TrackMap(struct EST_TrackMap::ChannelMappingElement map[])
{
  init();

  int i;
  for(i=0; map[i].type != channel_unknown; i++)
    set(map[i].type, map[i].channel);
}

EST_TrackMap::~EST_TrackMap()
{
}

short EST_TrackMap::get_parent(EST_ChannelType type) const
{
  short c= NO_SUCH_CHANNEL;
  if (p_parent!=0)
    {
      c = p_parent->get(type);
      if (c != NO_SUCH_CHANNEL)
	c -= p_offset;
    }
  return c;
}

EST_ChannelType EST_TrackMap::channel_type(unsigned short channel) const
{
  unsigned short i;

  for(i=0; i<num_channel_types;i++)
    if (p_map[i] == channel)
      return (EST_ChannelType)i;

  if (p_parent!=0)
    return p_parent->channel_type(channel+p_offset);

  return channel_unknown;
}

short EST_TrackMap::last_channel(void) const
{
  short last = -1;
  for(short i=0; i<num_channel_types;i++)
    if (p_map[i]> last)
      last = p_map[i];
  return last;
}


static EST_TValuedEnumDefinition<EST_ChannelType, const char *, NO_INFO> channel_name_tbl[] = {
  { channel_unknown,	{ "Unknown" }},
  { channel_order,	{ "Order" }},
  { channel_power,	{ "power", "Power", "raw_power"}},
  { channel_power_d,	{ "power_d", "Power_d", "raw_power_d"}},
  { channel_power_a,	{ "power_a", "Power_a", "raw_power_a"}},
  { channel_energy,	{ "energy", "Energy"}},
  { channel_energy_d,	{ "energy_d", "Energy_d"}},
  { channel_energy_a,	{ "energy_a", "Energy_a"}},
  { channel_peak,	{ "Peak", "ac_peak" }},
  { channel_duration,	{ "Duration" }},
  { channel_length,	{ "Length", "frame_len"}},
  { channel_offset,	{ "Offset", "frame_offset"}},
  { channel_f0,		{ "f0", "F0" }},
  { channel_f0_d,	{ "f0_d", "F0_d" }},
  { channel_f0_a,	{ "f0_a", "F0_a" }},
  { channel_voiced,	{ "Voiced", "prob_voice"}},
  { channel_frame,	{ "Frame"}},
  { channel_time,	{ "Time"}},
  { channel_lpc_0,	{ "lpc_0", "Lpc_0"}},
  { channel_lpc_N,	{ "lpc_N", "Lpc_N"}},
  { channel_lpc_d_0,	{ "lpc_d_0"}},
  { channel_lpc_d_N,	{ "lpc_d_N"}},
  { channel_lpc_a_0,	{ "lpc_a_0"}},
  { channel_lpc_a_N,	{ "lpc_a_N"}},
  { channel_cepstrum_0,	{ "cep_0"}},
  { channel_cepstrum_N,	{ "cep_N"}},
  { channel_cepstrum_d_0,	{ "cep_d_0"}},
  { channel_cepstrum_d_N,	{ "cep_d_N"}},
  { channel_cepstrum_a_0,	{ "cep_a_0"}},
  { channel_cepstrum_a_N,	{ "cep_a_N"}},
  { channel_melcepstrum_0,	{ "melcep_0"}},
  { channel_melcepstrum_N,	{ "melcep_N"}},
  { channel_melcepstrum_d_0,	{ "melcep_d_0"}},
  { channel_melcepstrum_d_N,	{ "melcep_d_N"}},
  { channel_melcepstrum_a_0,	{ "melcep_a_0"}},
  { channel_melcepstrum_a_N,	{ "melcep_a_N"}},
  { channel_lsf_0,	{ "lsf_0"}},
  { channel_lsf_N,	{ "lsf_N"}},
  { channel_lsf_d_0,	{ "lsf_d_0"}},
  { channel_lsf_d_N,	{ "lsf_d_N"}},
  { channel_lsf_a_0,	{ "lsf_a_0"}},
  { channel_lsf_a_N,	{ "lsf_a_N"}},
  { channel_fbank_0,	{ "fbank_0"}},
  { channel_fbank_N,	{ "fbank_N"}},
  { channel_fbank_d_0,	{ "fbank_d_0"}},
  { channel_fbank_d_N,	{ "fbank_d_N"}},
  { channel_fbank_a_0,	{ "fbank_a_0"}},
  { channel_fbank_a_N,	{ "fbank_a_N"}},
  { channel_filter_0,	{ "filter_0"}},
  { channel_filter_N,	{ "filter_N"}},
  { channel_filter_d_0,	{ "filter_d_0"}},
  { channel_filter_d_N,	{ "filter_d_N"}},
  { channel_filter_a_0,	{ "filter_a_0"}},
  { channel_filter_a_N,	{ "filter_a_N"}},
  { channel_reflection_0,	{ "reflection_0"}},
  { channel_reflection_N,	{ "reflection_N"}},
  { channel_reflection_d_0,	{ "reflection_d_0"}},
  { channel_reflection_d_N,	{ "reflection_d_N"}},
  { channel_reflection_a_0,	{ "reflection_a_0"}},
  { channel_reflection_a_N,	{ "reflection_a_N"}},
  { channel_unknown,	{ NULL }}
};

EST_ChannelNameMap EST_default_channel_names(channel_name_tbl);

static EST_TValuedEnumDefinition<EST_ChannelType, const char *, NO_INFO> esps_channel_name_tbl[] = {
  { channel_unknown,	{ "Unknown" }},
  { channel_order,	{ "Order" }},
  { channel_power,	{ "raw_power", "rms" }},
  { channel_peak,	{ "ac_peak" }},
  { channel_duration,	{ "Duration" }},
  { channel_length,	{ "frame_len"}},
  { channel_f0,		{ "F0" }},
  { channel_voiced,	{ "prob_voice"}},
  { channel_lpc_0,	{ "lpc_0", "spec_param" }},
  { channel_lpc_N,	{ "lpc_N"}},
  //   { channel_coef0,	{ "spec_param" }},
  { channel_unknown,	{ NULL }}
};

EST_ChannelNameMap esps_channel_names(esps_channel_name_tbl);

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TNamedEnum.cc"

template class EST_TNamedEnum<EST_ChannelType>;
template class EST_TNamedEnumI<EST_ChannelType, NO_INFO>;
template class EST_TValuedEnum<EST_ChannelType,const char *>;
template class EST_TValuedEnumI<EST_ChannelType,const char *, NO_INFO>;

template class EST_THandle<EST_TrackMap,EST_TrackMap>;

#endif
