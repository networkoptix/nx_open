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
 /*                   Date: Fri May  9 1997                               */
 /* -------------------------------------------------------------------   */
 /* Example of declaration and use of tracks.                             */
 /*                                                                       */
 /*************************************************************************/


#include <iostream>
#include <cstdlib>
#include "EST_Track.h"
#include "EST_Wave.h"
#include "EST_sigpr.h"
#include "EST_error.h"


/** @name EST_Track class example code
  * @toc
  * Some examples of track manipulations.
  *
  */
//@{

int main(void)

{
    int i, j;

    /* This program is designed as an example not as something to run
       so for testing purpose it simply exists */
    exit(0);
    /**@name Initialising and Resizing a Track

       The constructor functions can be used to create a track with
       zero frames and channels or a track with a specified number of
       frames and channels
    */

    //@{
    //@{ code
    EST_Track tr;           // default track declaration
    EST_Track tra(500, 10); // allocate track with 500 frames and 10 channels
    //@} code

    /** tracks can be resized at any time:
     */
    //@{ code
    tr.resize(10, 500); // resize track to have 10 frames and 500 channels
    tr.resize(500, 10); // resize track to have 500 frames and 10 channels
    //@} code

    /** by default, resizing preserves values in the track. This
	may involve copying some information, so if the existing values
	are not needed, a flag can be set which usually results in 
	quicker resizing
    */
    //@{ code
    tr.resize(250, 5, 0);  // throw away any existing values
    //@} code
    /** If only the number of channels or the number of frames needs
	to be changed, this an be done with the following functions:
    */

    //@{ code
    tr.set_num_channels(10);   // makes 10 channels, keeps same no of frames

    tr.set_num_frames(400);    // makes 400 frames, keeps same no of channels
    //@} code
    /** The preserve flag works in the same way with these functions 
     */
    //@}

    /** @name Simple Access
     
	Values in the track can be accessed and set by frame
	number and channel number.

	The following resizes a track to have 500 frames and 10 channels
	and fills every position with -5.
    */
    //@{
    //@{ code
    tr.resize(500, 10); 

    for (i = 0; i < tr.num_frames(); ++i)
	for (j = 0; j < tr.num_channels(); ++j)
	    tr.a(i, j) = -5.0;

    //@} code

    /** A well formed track will have a time value, specified in seconds,
	for every frame. The time array can be filled directly:
    */
    //@{ code
    for (i = 0; i < tr.num_frames(); ++i)
	tr.t(i) = (float) i * 0.01;
    //@} code
    /** which fills the time array with values 0.01, 0.02,
	0.03... 5.0. However, A shortcut function is provided for fixed
	frame spacing: 
    */
    //@{ code
    tr.fill_time(0.1);

    //@} code
    /** which performs the same operation as above. Frames do not have
	to be evenly spaced, in pitch synchronous processing the time
	array holds the time position of each pitch period. In such
	cases each position in the time array must obviously be set
	individually.</para><para>

	Some representations have undefined values during certain
	sections of the track, for example the F0 value during
	unvoiced speech.</para><para>

	The break/value array can be used to specify if a frame has an
	undefined value.<para></para>. If a frame in this array is 1,
	that means the amplitude is defined at that point. If 0, the
	amplitude is undefined. By default, every frame has a value.
	</para><para>

	Breaks (undefined values) can be set by <method>set_break()
	</method>. The following sets every frame from 50 to 99 as a
	break:
    */
    //@{ code
    for (i = 50; i < 100; ++i)
	tr.set_break(i);
    //@} code
    /** frames can be turned back to values as follows:
     */
    //@{ code
    for (i = 50; i < 100; ++i)
	tr.set_value(i);
    //@} code
    /** It is up to individual functions to decide how to interpret breaks.
	</para><para>
	A frame's status can be checked as follows:
    */
    //@{ code
    if (tr.val(60))
	cout << "Frame 60 is not a break\n";

    if (tr.track_break(60))
	cout << "Frame 60 is a break\n";
    //@} code
    //@}

    /** @name Naming Channels
	@id tr-example-naming-channels

	While channels can be accessed by their index, it is often useful
	to give them names and refer to them by those names.

	The set_channel_name() function sets the name of a single channel:
    */
    //@{
    //@{ code
    tr.set_channel_name("F0", 0); 
    tr.set_channel_name("energy", 1); 
    //@} code

    /** An alternative is to use a predefined set of channel names
	stored in a <emphasis>map</emphasis>.A track map
	is simply a String List strings which describe a channel name
	configuration. The <method>resize</method> function can take
	this and resize the number of channels to the number of channels
	indicated in the map, and give each channel its name from the 
	map. For example:
    */
    //@{ code
    EST_StrList map;
    map.append("F0");
    map.append("energy");

    tr.resize(500, map); // this makes a 2 channel track and sets the names to F0 and energy
    //@} code

    /** A convention is used for channels which comprise
	components of a multi-dimensional analysis such as
	cepstra. In such cases the channels are named
	<replaceable>TYPE_I</replaceable>.  The last coefficient is
	always names <replaceable>TYPE_N</replaceable> regardless of
	the number of coefficients. This is very useful in extracting
	a set of related  channels without needing to know the order
	of the analysis.

	For example, a track map might look like:

    */
    //@{ code

    map.clear();
    map.append("F0");
    map.append("energy");

    map.append("cep_0");
    map.append("cep_1");
    map.append("cep_2");
    map.append("cep_3");
    map.append("cep_4");
    map.append("cep_5");
    map.append("cep_6");
    map.append("cep_7");
    map.append("cep_N");

    tr.resize(500, map); // makes a 11 channel track and sets the names
    //@} code

    /** This obviously gets unwieldy quite quickly, so the mapping
	mechanism provides a short hand for multi-dimensional data.

    */

    //@{ code
    map.clear();
    map.append("F0");
    map.append("energy");

    map.append("$cep-0+8");

    tr.resize(500, map); // does exactly as above
    //@} code

    /** Here $ indicates the special status, "cep" the name of the
	coefficients, "-0" that the first is number 0 and "+8" that
	there are 8 more to follow. 
    */

    //@}


    /** @name Access single frames or single channels.

	@id tr-example-frames-and-channels
      
	Often functions perform their operations on only a single
	frame or channel, and the track class provides a general
	mechanism for doing this.

	Single frames or channels can be accessed as EST_FVectors:
	Given a track with 500 frames and 10 channels, the 50th frame
	can be accessed as:
    */
    //@{
    //@{ code
    EST_FVector tmp_frame;

    tr.frame(tmp_frame, 50);
    //@} code
    /** now tmp_frame is 10 element vector, which is
	a window into tr: any changes to the contents of tmp_frame will
	change tr. tmp_frame cannot be resized. (This operation can
	be thought in standard C terms as tmp_frame being a pointer
	to the 5th frame of tr).
	</para>	<para>
	Likewise with channels:
    */
    //@{ code
    EST_FVector tmp_channel;

    tr.channel(tmp_channel, 5);
    //@} code
    /** Again, tmp_channel is 500 element vector, which is
	a window into tr: any changes to the contents of tmp_channel will
	change tr. tmp_channel cannot be resized. 
	</para><para>
	Channels can also be extracted by name:
    */
    //@{ code
    tr.channel(tmp_channel, "energy");
    //@} code
    /** not all the channels need be put into the temporary frame.
	Imagine we have a track with a F0 channel,a energy channel and
	10 cepstrum channels. The following makes a frame from the
	50th frame, which only includes the cepstral information in
	channels 2 through 11 */
    //@{ code
    tr.frame(tmp_frame, 50, 2, 9);
    //@} code
    /** Likewise, the 5th channel with only the last 100 frames can be set up
	as: */
    //@{ code
    tr.channel(tmp_channel, 5, 400, 100);
    //@} code
    //@}
    /** @name Access multiple frames or channels.
	@id tr-example-sub-tracks
	In addition to extracting single frames and channels, multiple
	frame and channel portions can be extracted in a similar
	way. In the following example, we make a sub-track sub, which
	points to the entire cepstrum portion of a track (channels 2
	through 11) 
    */
    //@{
    //@{ code
    EST_Track sub;

    tr.sub_track(sub, 0, EST_ALL, 2, 9);

    //@} code

    /** <parameter>sub</parameter> behaves exactly like a normal
	track in every way, except that it cannot be resized. Its
	contents behave like a point into the designated portion of
	<parameter>tr</parameter>, so changing
	<parameter>sub</parameter> will change<parameter>
	tr</parameter>.

	</para><para> The first argument is the
	<parameter>sub</parameter> track. The second states the start
	frame and the total number of frames required. EST_ALL is a
	special constant that specifies that all the frames are
	required here. The next argument is the start channel number
	(remember channels are numbered from 0), and the last argument
	is the total number of channels required.  </para><para>

	This facility is particularly useful for using standard
	signal processing functions efficiently. For example,
	the <function>melcep</function> in the signal processing library
	takes a waveform and produces a mel-scale cepstrum. It determines
	the order of the cepstral analysis by the number of channels in
	the track it is given, which has already been allocated to have
	the correct number of frames and channels.

	</para><para> The following will process the waveform
	<parameter>sig</parameter>, produce a 10th order mel cepstrum
	and place the output in <parameter>sub</parameter>. (For
	explanation of the other options see
	<function>melcep</function> */
    //@{ code
    EST_Wave sig;

    melcep(sig, sub, 1.0, 20, 22); 
    //@} code

    /** because we have made<parameter>sub</parameter> a window
	into<parameter> tr</parameter>, the melcep function writes its
	output into the correct location, i.e. channels 2-11 of tr. If
	it were no for the sub_track facility, either a separate track
	of the right size would be passed into melcep and then it
	would be copied into tr (wasteful), or else tr would be passed
	in and other arguments would have to specify which channels
	should be written to (messy).  </para><para> 

	Sub-tracks can also be set using channel names. The
	following example does exactly as above, but is referenced by
	the name of the first channel required and the number of
	channels to follow: */
    //@{ code

    tr.sub_track(sub, 0, EST_ALL, "cep_0", "cep_N");
    //@} code
    /** and this specifies the end by a string also:
     */
    //@{ code
    tr.sub_track(sub, 0, EST_ALL, "cep_0", "cep_N");
    //@} code
    /** sub_tracks can be any set of continuous frames and
        channels. For example if a word started at frame 43 and ended
        and frame 86, the following would set a sub track to that
        portion: */
    //@{ code

    tr.sub_track(sub, 47, 39, "cep_0", "cep_N");

    //@} code

    /** We can step through the frames of a Track using a standard
     * iterator. The frames are returned as one-frame sub-tracks.
     */

    //@{ code
    EST_Track::Entries frames;

    // print out the time of every 50th track
    cout << "Times:";

    for (frames.begin(tr); frames; ++frames)
      {
	const EST_Track &frame = *frames;
	if (frames.n() % 50 ==0)
	    cout << " " << frames.n() << "[" << frame.t() << "]";
    }
    cout << "\n";
	   
    //@} code

    /** The <function>channel</function>, <function>frame</function>
	and <function>sub_track</function> functions are most commonly
	used to write into a track using a convenient
	sub-portion. Sometimes, however a simple copy is required
	whose contents can be written without affecting the original.

	The <member>copy_cub_track</member> function does this */
    //@{ code
    EST_Track tr_copy;
    
//    tr.copy_sub_track(tr_copy, 47, 39, "cep_0", "cep_N");
    //@} code

    /** Individual frames and channels can be copied out into
	pre-allocated float * arrays as follows:
    */
    //@{ code
    float *channel_buf, *frame_buf;
    channel_buf = new float[tr.num_frames()];
    frame_buf = new float[tr.num_channels()];

    tr.copy_channel_out(5, channel_buf);   // copy channel 5 into channel_buf
    tr.copy_frame_out(43, frame_buf);      // copy frame 4 into frame_buf
    //@} code

    /** Individual frames and channels can be copied into the track
	from float * arrays as follows:
    */
    //@{ code
    tr.copy_channel_in(5, channel_buf);    // copy channel_buf into channel 5 
    tr.copy_frame_in(43, frame_buf);       // copy frame_buf into frame 4
    //@} code
    //@}


    /** @name Auxiliary Channels
	Auxiliary channels are used for storing frame information other than
	amplitude coefficients, for example voicing decisions and points of
	interest in the track.

	Auxiliary channels always have the same number of frames as the
	amplitude channels. They are resized by assigning names to the
	channels that need to be created:
    */
    //@{
    //@{ code


    EST_StrList aux_names;

    aux_names.append("voicing");
    aux_names.append("join_points");
    aux_names.append("cost");

    tr.resize_aux(aux_names);

    //@} code
    /** The following fills in these three channels with some values:
     */
    //@{ code

    for (i = 0; i < 500; ++i)
    {
	tr.aux(i, "voicing") = i;
	tr.aux(i, "join_points") = EST_String("stuff");
	tr.aux(i, "cost") =  0.111;
    }
    //@} code
    //@}

    /** @name File I/O 
	Tracks in various formats can be saved and loaded:

	Save as a HTK file:
    */
    //@{
    //@{ code
    if (tr.save("tmp/track.htk", "htk") != write_ok)
	EST_error("can't save htk file\n");
    //@} code
    /** Save as a EST file:
     */
    //@{ code
    if (tr.save("tmp/track.est", "est") != write_ok)
	EST_error("can't save est file\n");
    //@} code
    /** Save as an ascii file:
     */
    //@{ code
    if (tr.save("tmp/track.ascii", "ascii") != write_ok)
	EST_error("can't save ascii file\n");
    //@} code
    /** The file type is automatically determined from the file's
	header during loading:
    */
    //@{ code

    EST_Track tr2;
    if (tr2.load("tmp/track.htk") != read_ok)
	EST_error("can't reload htk\n");
    //@} code

    /** If no header is found, the function assumes the
	file is ascii data, with a fixed frame shift, arranged with rows
	representing frames and columns channels. In this case, the
	frame shift must be specified as an argument to this function:
    */
    //@{ code
    if (tr.load("tmp/track.ascii", 0.01) != read_ok)
	EST_error("can't reload ascii file\n");
    //@} code
    //@}

    exit(0);
}

//@}







