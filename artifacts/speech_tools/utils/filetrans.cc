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
/*                Author :  Alan Black                                   */
/*                Date   :  December 1996                                */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* File transfer over open file descriptors.  Uses key stuffing          */
/* to allow transfer of any file over an open link (usually a socket)    */
/*                                                                       */
/*=======================================================================*/
#include "EST_unix.h"
#include <cstdio>
#include "EST_String.h"
#include "EST_io_aux.h"

// The following key is used when stuffing files down a socket.
// This key in when received denotes the end of file.  Any occurrence
// of key in the file with have X inserted in it, and the receiving end
// with remove that X in the file.  This is a technique I learned from
// HDLC network protocols which guarantees 0x7e (6 bits) starts a header.
// This allows transfer of files even if they include the stuff key.
const char *file_stuff_key = "ft_StUfF_key";

static int getc_unbuffered(SOCKET_FD fd)
{
    // An attempted to get rid of the buffering
    char c;
    int n;

#ifdef WIN32
    n = recv(fd,&c,1,0);
#else
    n = read(fd,&c,1);
#endif

    if (n == 0)      // this isn't necessarily eof
	return EOF;
    else
	return c;    // and this might be -1 (EOF) 
}

int socket_receive_file(SOCKET_FD fd,const EST_String &filename)
{
    // Copy the file from fd to filename using the 7E stuff key
    // mechanism so binary files may pass through the socket
    // without signals or eof.
    FILE *outfd;
    int k,i,c;

    if ((outfd=fopen(filename,"wb")) == NULL)
    {
	cerr << "socket_receive_file: can't find file \"" <<
	    filename << "\"\n";
	return -1;
    }

    k=0;
    while (file_stuff_key[k] != '\0')
    {
	c = getc_unbuffered(fd);
	if (file_stuff_key[k] == c)
	    k++;
	else if ((c == 'X') && (file_stuff_key[k+1] == '\0'))
	{
	    for (i=0; i < k; i++) putc(file_stuff_key[i],outfd);
	    k=0;
	    // omit the stuffed 'X'
	}
	else
	{
	    for (i=0; i < k; i++) putc(file_stuff_key[i],outfd);
	    k=0;
	    putc(c,outfd);
	}
    }
    fclose(outfd);
    return 0;
}

int socket_send_file(SOCKET_FD fd,const EST_String &filename)
{
    // Send file down fd using the 7E end stuffing technique.
    // This guarantees the binary transfer without any other
    // signals eof etc
#ifndef WIN32
    FILE *ffd = fdopen(dup(fd),"wb");   // use some buffering
#endif
    FILE *infd;
    int k,c;

    if ((infd=fopen(filename,"rb")) == NULL)
    {
	cerr << "socket_send_file: can't find file \"" <<
	    filename << "\"\n";
	return -1;
    }

    k=0;
    while ((c=getc(infd)) != EOF)
    {
	if (file_stuff_key[k] == c)
	    k++;
	else
	    k=0;
	if (file_stuff_key[k] == '\0')
	{
#ifdef WIN32
	    const char filler='X';
	    send(fd,&filler,1,0);
#else
	    putc('X',ffd);   // stuff filler
#endif
	    k=0;
	}
#ifdef WIN32
	send(fd,(const char *)&c,1,0);
#else
	putc(c,ffd);
#endif
    }
    for (k=0; file_stuff_key[k] != '\0'; k++)
#ifdef WIN32
	send(fd,file_stuff_key+k,1,0);
#else
	putc(file_stuff_key[k],ffd);      // stuff whole key as its the end

    fflush(ffd);
    fclose(ffd);
#endif
    fclose(infd);
    return 0;
}
