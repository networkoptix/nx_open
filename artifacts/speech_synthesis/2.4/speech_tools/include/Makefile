###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                         Copyright (c) 1996                            ##
##                        All Rights Reserved.                           ##
##                                                                       ##
##  Permission is hereby granted, free of charge, to use and distribute  ##
##  this software and its documentation without restriction, including   ##
##  without limitation the rights to use, copy, modify, merge, publish,  ##
##  distribute, sublicense, and/or sell copies of this work, and to      ##
##  permit persons to whom this work is furnished to do so, subject to   ##
##  the following conditions:                                            ##
##   1. The code must retain the above copyright notice, this list of    ##
##      conditions and the following disclaimer.                         ##
##   2. Any modifications must be clearly marked as such.                ##
##   3. Original authors' names are not deleted.                         ##
##   4. The authors' names are not used to endorse or promote products   ##
##      derived from this software without specific prior written        ##
##      permission.                                                      ##
##                                                                       ##
##  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
##  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
##  THIS SOFTWARE.                                                       ##
##                                                                       ##
###########################################################################
##                   Include Directory Makefile                          ##
###########################################################################

TOP=..
DIRNAME=include
H = EST_common.h EST_TKVL.h EST_Option.h EST_TList.h \
    EST_UList.h EST_TSortable.h \
    EST_Token.h EST_StringTrie.h \
    EST_TMatrix.h EST_TVector.h EST_TSimpleMatrix.h EST_TSimpleVector.h \
    EST_FMatrix.h EST_DMatrix.h EST_types.h EST_cmd_line.h EST.h  \
    EST_IMatrix.h EST_SMatrix.h \
    EST_sigpr.h EST_Event.h EST_tilt.h \
    EST_Track.h EST_io_aux.h EST_audio.h \
    EST_track_aux.h EST_cluster.h EST_multistats.h EST_PST.h\
    EST_cutils.h EST_walloc.h EST_math.h  EST_Regex.h \
    EST_String.h EST_string_aux.h EST_wave_aux.h EST_Chunk.h \
    EST_Ngrammar.h EST_sort.h \
    EST_rw_status.h EST_Wave.h EST_SingleChannelWave.h EST_TNamedEnum.h EST_grammar.h \
    EST_util_class.h EST_speech_class.h EST_ling_class.h EST_stats.h \
    EST_simplestats.h EST_Pathname.h EST_viterbi.h \
    EST_lattice_io.h EST_lattice.h EST_HMM.h EST_bool.h \
    EST_THash.h EST_TrackMap.h EST_ChannelType.h \
    EST_cmd_line_options.h EST_strcasecmp.h EST_error.h \
    EST_TBuffer.h EST_Wagon.h EST_Val.h EST_Val_defs.h \
    EST_system.h EST_unix.h EST_socket.h EST_iostream.h \
    siod.h siod_defs.h siod_est.h \
    EST_SCFG.h EST_SCFG_Chart.h EST_WFST.h EST_Complex.h \
    EST_inline_utils.h EST_Contents.h EST_FileType.h \
    EST_Features.h EST_features_aux.h EST_Featured.h EST_TTimeIndex.h \
    EST_THandle.h EST_Handleable.h EST_TBox.h \
    EST_TIterator.h EST_kalman.h EST_TDeque.h \
    EST_model_types.h EST_dynamic_model.h EST_FeatureData.h \
    EST_ServiceTable.h EST_Server.h EST_FringeServer.h 

DEVELOPMENT_DIRS = # development

ALL_DIRS = unix win32 sigpr ling_class instantiate rxp $(DEVELOPMENT_DIRS)

FILES = Makefile $(H)
INSTALL = install_incs

include $(TOP)/config/common_make_rules

install_incs:
	mkdir -p $(INCDIR)/EST/
	cp -p *.h $(INCDIR)/EST/
