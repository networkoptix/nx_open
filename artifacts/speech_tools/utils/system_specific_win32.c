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
/* functions which mimic unix system calls                               */

#include <stdlib.h>
#include <stdio.h>
#include "EST_unix.h"
#include "EST_socket.h"

int unix_access(const char *file, int mode)
{
  DWORD flags = GetFileAttributes(file);

  if (flags == 0xffffffff)
  {
      if (ERROR_FILE_NOT_FOUND == GetLastError())
	  return -1;
      else return 1;
  }

  if (mode==F_OK)
    return 0;

  if (mode==R_OK)
    return 0;

  if (mode==W_OK)
  {
   /* When referring to a directory, FILE_ATTRIBUTE_READONLY means the directory can't be
      deleted.  It does not mean a file can't be written to the directory.
   */
	if (flags|FILE_ATTRIBUTE_DIRECTORY)
      return 0;
	else 
      return (flags|FILE_ATTRIBUTE_READONLY) != 0;
  }

  return 1;
}


int unix_read(HANDLE fd, char *buffer, int n)
{
  int howmany;

  if (ReadFile(fd, buffer, n, &howmany, NULL))
    return howmany;

  return -1;
}

char *unix_getcwd(char *buffer, int maxlength)
{
  static char lbuffer[1024];

  if (buffer==NULL)
    {
    buffer = lbuffer;
    maxlength=1023;
    }

  if (GetCurrentDirectory(maxlength, buffer) >=0)
    return buffer;
  return NULL;
}

int unix_waitpid(int pid, int *statusp, int flags)
{
  fprintf(stderr, "waitpid not yet implemented\n");
  return 0;
}

int unix_fork(void)
{
  fprintf(stderr," FORK NOT YET IMPLEMENTED\n");
  return -1;
}

static int sockets_initialised =0;

int socket_initialise(void)
{
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  if (sockets_initialised)
    return 1;
  
  wVersionRequested = MAKEWORD( 2, 0 );
 
  err = WSAStartup( wVersionRequested, &wsaData );
  if ( err != 0 ) 
    {
      printf("Socket Initialisation failed\n");
      return 0;
    }

  sockets_initialised=1;
  return 1;
}
