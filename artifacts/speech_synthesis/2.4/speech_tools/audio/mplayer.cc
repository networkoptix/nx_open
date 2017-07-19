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
/*                Author :  Richard Caley                                */
/*                Date   :  August 1997                                  */
/*-----------------------------------------------------------------------*/
/*  Optional 16bit linear support for Windows NT/95                      */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_Option.h"
#include "EST_io_aux.h"
#include "EST_Pathname.h"

#ifdef SUPPORT_MPLAYER

int mplayer_supported = TRUE;

static EST_Pathname tempfile;
static EST_Pathname temppath;
static EST_Pathname tempdir;

static void find_tempfile(void)
{
  const char *s=NULL;

  if (!(s=getenv("TMP")))
    if (!(s=getenv("TMPDIR")))
      if (!(s=getenv("TEMPDIR")))
	s="\tmp";

  tempdir = s;

  char buffer[512];
  sprintf(buffer, "est%4d.wav", getpid());
  tempfile = buffer;
  temppath = EST_Pathname::append(tempdir.as_directory(), tempfile);
}

int play_mplayer_wave(EST_Wave &inwave, EST_Option &al)
{

  if (tempfile == "")
    find_tempfile();

  inwave.save(temppath, "riff");

  char command[1000];

  sprintf(command, 
	  "mplayer /play %s\\%s", 
	  (const char *)tempdir, 
	  (const char *)tempfile);

  cout << "command '" << command <<"'\n";
  system(command);

  unlink(temppath);

  return 1;
}

#else
int mplayer_supported = FALSE;

int play_mplayer_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "Windows mplayer not supported" << endl;
    return -1;
}

#endif
