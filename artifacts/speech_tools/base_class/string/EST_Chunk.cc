
 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                        Copyright (c) 1997                            */
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
 /*                                                                      */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)            */
 /*                   Date: February 1997                                */
 /* -------------------------------------------------------------------- */
 /*                                                                      */
 /* Use counted memory chunks and smart pointers to them.                */
 /*                                                                      */
 /************************************************************************/

#include <cstdlib>
#include <iostream>
#include <cstring>
#include "EST_Chunk.h"

EST_Chunk::EST_Chunk ()
{
  count = 0;
  memory[0] = '\0';
  //  cerr<<"created " << hex << (int)&memory << "," << dec << size <<"\n";
}

EST_Chunk::~EST_Chunk ()
{
  if (count > 0)
    {
      cerr << "deleting chunk with non-zero count\n";
      exit(1);
    }

  //  cerr << "deleted "<< hex << (int)&memory << "," << dec << size <<"\n";
}

// private address-of operator - up to friends to keep use counts correct.

EST_Chunk *EST_Chunk::operator & ()
{
  return this;
}

#if !defined(__CHUNK_INLINE_AGGRESSIVELY__)

void EST_Chunk:: operator ++ ()
{
#if 0
  if (++count > MAX_CHUNK_COUNT) 
    { 
      cerr<<"max count exceeded\n";
      exit(1);
    }
#endif

  if (count < MAX_CHUNK_COUNT) 
  {
      ++count; 
  }
}

void EST_Chunk::operator -- ()
{
  if (count-- == 0) 
    { 
      cerr<<"negative count\n";
      exit(1);
    }
  else if (count == 0)
    {
      //  cerr<<"deleting\n";
      delete this;
    }
}
#endif

void *EST_Chunk::operator new (size_t size, int bytes)
{

  if (bytes > MAX_CHUNK_SIZE)
    {
      cerr<<"trying to make chunk of size "<<bytes<<"\n";
    }

#if defined(__CHUNK_USE_WALLOC__)
  void *it = walloc(char, size+bytes);
  ((EST_Chunk *)it) -> malloc_flag = 1;
#else
  void *it = new char[size + bytes];
#endif

  //  cerr<<"allocated "<<bytes+size<<" byte for chunk\n";

  ((EST_Chunk *)it) -> size = bytes;

  return it;
}

void EST_Chunk::operator delete (void *it)
{

#if defined(__CHUNK_USE_WALLOC__)
  wfree(it);
#else
  delete it;
#endif

}

 /************************************************************************/
 /*                                                                      */
 /* Now the smart pointers.                                              */
 /*                                                                      */
 /************************************************************************/

#if !defined(__CHUNK_INLINE_AGGRESSIVELY__)

EST_ChunkPtr::EST_ChunkPtr (EST_Chunk *chp)
{
  ptr=chp;
  if (ptr)
    ++ *ptr;
}

EST_ChunkPtr::EST_ChunkPtr (const EST_ChunkPtr &cp)
{
  ptr=cp.ptr;
  if (ptr)
    ++ *ptr;
}

EST_ChunkPtr::~EST_ChunkPtr (void)
{
  if (ptr)
  {
    -- *ptr;
  }
}

EST_ChunkPtr &EST_ChunkPtr::operator = (EST_ChunkPtr cp)
{
  // doing it in this order means self assignment is safe.
  if (cp.ptr)
    ++ *(cp.ptr);
  if (ptr)
    -- *ptr;
  ptr=cp.ptr;
  return *this;
}

EST_ChunkPtr &EST_ChunkPtr::operator = (EST_Chunk *chp)
{
      // doing it in this order means self assignment is safe.
      if (chp)
	++ *chp;
      if (ptr)
	-- *ptr;
      ptr=chp;
      return *this;
}

EST_ChunkPtr::operator const char*() const 
{
  if (ptr)
    return &(ptr->memory[0]);
  else
    return NULL;
}

EST_ChunkPtr::operator char const*() 
{
    return ptr?&(ptr->memory[0]):(const char *)NULL;
}

EST_ChunkPtr::operator char*()
{
  if (ptr)
    {
      if (ptr->count > 1)
	{
	CHUNK_WARN("getting writable version of shared chunk");
	cp_make_updatable(*this); 
	}

      return &(ptr->memory[0]);
    }
  else
    return NULL;
}

char &EST_ChunkPtr::operator () (int i) { 
      if (ptr->count>1) 
	{
	  CHUNK_WARN("getting writable version of shared chunk");
	  cp_make_updatable(*this); 
	}
      return ptr->memory[i]; 
    }

#endif

 /************************************************************************/
 /*                                                                      */
 /* Friend function to allocate a chunk in a non count-aware program.    */
 /*                                                                      */
 /************************************************************************/

EST_ChunkPtr chunk_allocate(int bytes)
{
  EST_Chunk *cp = new(bytes) EST_Chunk;

  return (EST_ChunkPtr)cp;
}

EST_ChunkPtr chunk_allocate(int bytes, const char *initial, int initial_len)
{
  if (initial_len >= bytes)
    {
      cerr<<"initialiser too long\n";
      abort();
    }

  EST_Chunk *cp = new(bytes) EST_Chunk;

  memcpy(cp->memory, initial, initial_len);
  
  cp->memory[initial_len] = '\0';

  return (EST_ChunkPtr)cp;
}

EST_ChunkPtr chunk_allocate(int bytes, const EST_ChunkPtr &initial, int initial_start, int initial_len)
{
  if (initial_len >= bytes)
    {
    cerr<<"initialiser too long\n";
    abort();
    }

  EST_Chunk *cp = new(bytes) EST_Chunk;

  memcpy(cp->memory, initial.ptr->memory + initial_start, initial_len);
  
  cp->memory[initial_len] = '\0';

  return (EST_ChunkPtr)cp;
}

 /************************************************************************/
 /*                                                                      */
 /* And one which ensures that a chunk is not shared. Do this before     */
 /* writing to the memory. The first version is told how much of the     */
 /* memory to copy, the second just copies it all.                       */
 /*                                                                      */
 /************************************************************************/

void cp_make_updatable(EST_ChunkPtr &cp, EST_Chunk::EST_chunk_size inuse)
{
  if (cp.ptr && cp.ptr->count > 1)
    {
      EST_Chunk *newchunk = new(cp.ptr->size) EST_Chunk;

      memcpy(newchunk->memory, cp.ptr->memory, inuse);

      cp = newchunk;
    }
}

void cp_make_updatable(EST_ChunkPtr &cp)
{
  if (cp.ptr && cp.ptr->count > 1)
    {
      EST_Chunk *newchunk = new(cp.ptr->size) EST_Chunk;

      memcpy(newchunk->memory, cp.ptr->memory, cp.ptr->size);

      cp = newchunk;
    }
}

 /************************************************************************/
 /*                                                                      */
 /* Make more room in a chunk. If the chunk is already big enough and    */
 /* is unshared then nothing is done.                                    */
 /*                                                                      */
 /************************************************************************/

void grow_chunk(EST_ChunkPtr &cp, EST_Chunk::EST_chunk_size newsize)
{
  if (!cp.ptr || cp.ptr->size < newsize)
    {
      if (cp.ptr)
	cp_make_updatable(cp);
      EST_Chunk *newchunk = new(newsize) EST_Chunk;
      memcpy(newchunk->memory, cp.ptr->memory, cp.ptr->size);
      cp = newchunk;
    }
}

void grow_chunk(EST_ChunkPtr &cp, EST_Chunk::EST_chunk_size inuse, EST_Chunk::EST_chunk_size newsize)
{
  if (!cp.ptr || cp.ptr->size < newsize)
    {
      if (cp.ptr)
	cp_make_updatable(cp, inuse);
      EST_Chunk *newchunk = new(newsize) EST_Chunk;
      memcpy(newchunk->memory, cp.ptr->memory, inuse);
      cp = newchunk;
    }
}

ostream &operator << (ostream &s, const EST_Chunk &ch)
{
  char buff[21];

  if (ch.size<20)
    {
    memcpy(buff, ch.memory, ch.size);
    buff[ch.size]='\0';
    }
  else
    {
    memcpy(buff, ch.memory, 20);
    buff[20]='\0';
    }
    
  return (s<< "[" << ch.size << "!" << ch.count << "!" << buff << "]");
}


