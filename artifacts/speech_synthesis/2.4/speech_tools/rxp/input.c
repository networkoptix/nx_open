/*************************************************************************/
/*                                                                       */
/* Copyright (c) 1997-98 Richard Tobin, Language Technology Group, HCRC, */
/* University of Edinburgh.                                              */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,     */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF EDINBURGH BE LIABLE */
/* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF    */
/* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION    */
/* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       */
/*                                                                       */
/*************************************************************************/
/* 
 * This code is in a distressed state due to hackery for windoze.
 * See comment in url.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef FOR_LT

#include "lt-memory.h"
#include "nsllib.h"

#define ERR(m) LT_ERROR(NECHAR,m)
#define ERR1(m,x) LT_ERROR1(NECHAR,m,x)
#define ERR2(m,x,y) LT_ERROR2(NECHAR,m,x,y)
#define ERR3(m,x,y,z) LT_ERROR3(NECHAR,m,x,y,z)

#define Malloc salloc
#define Realloc srealloc
#define Free sfree

#else

#include "system.h"
#define ERR(m) fprintf(stderr,m)
#define ERR1(m,x) fprintf(stderr,m,x)
#define ERR2(m,x,y) fprintf(stderr,m,x,y)
#define ERR3(m,x,y,z) fprintf(stderr,m,x,y,z)

#endif

#include "charset.h"
#include "string16.h"
#include "dtd.h"
#include "input.h"
#include "url.h"
#include "ctype16.h"

static int get_translated_line1(InputSource s);

InputSource SourceFromStream(const char8 *description, FILE *file)
{
    Entity e;

    e = NewExternalEntity(0, 0, description, 0, 0);
    if(!strchr8(description, '/'))
	EntitySetBaseURL(e, default_base_url());
    
    return NewInputSource(e, MakeFILE16FromFILE(file, "r"));
}

InputSource EntityOpen(Entity e)
{
    FILE16 *f16;

    if(e->type == ET_external)
    {
	const char8 *url = EntityURL(e);

	if(!url || !(f16 = url_open(url, 0, "r", 0)))
	    return 0;
    }
    else
    {
	f16 = MakeFILE16FromString((char *)e->text, -1, "r");
    }

    return NewInputSource(e, f16);
}


InputSource NewInputSource(Entity e, FILE16 *f16)
{
    InputSource source;

    if(!(source = Malloc(sizeof(*source))))
	return 0;

    source->line = 0;
    source->line_alloc = 0;
    source->line_length = 0;
    source->next = 0;
    source->seen_eoe = 0;

    source->entity = e;

    source->file16 = f16;

    source->bytes_consumed = 0;
    source->bytes_before_current_line = 0;
    source->line_end_was_cr = 0;
    source->line_number = 0;
    source->not_read_yet = 1;

    source->nextin = source->insize = 0;

    source->parent = 0;

    return source;
}

int SourceLineAndChar(InputSource s, int *linenum, int *charnum)
{
    Entity e = s->entity, f = e->parent;

    if(e->type == ET_external)
    {
	*linenum = s->line_number;
	*charnum = s->next;
	return 1;
    }

    if(f && f->type == ET_external)
    {
	if(e->matches_parent_text)
	{
	    *linenum = e->line_offset + s->line_number;
	    *charnum = (s->line_number == 0 ? e->line1_char_offset : 0) +
		       s->next;
	    return 1;
	}
	else
	{
	    *linenum = e->line_offset;
	    *charnum = e->line1_char_offset;
	    return 0;
	}
    }

    if(f && f->matches_parent_text)
    {
	*linenum = f->line_offset + e->line_offset;
	*charnum = (e->line_offset == 0 ? f->line1_char_offset : 0) +
	    e->line1_char_offset;
	return 0;
    }

    return -1;
}

void SourcePosition(InputSource s, Entity *entity, int *byte_offset)
{
    *entity = s->entity;
    *byte_offset = SourceTell(s);
}

int SourceTell(InputSource s)
{
#if CHAR_SIZE == 8
    return s->bytes_before_current_line + s->next;
#else
    switch(s->entity->encoding)
    {
    case CE_ISO_10646_UCS_2B:
    case CE_UTF_16B:
    case CE_ISO_10646_UCS_2L:
    case CE_UTF_16L:
	return s->bytes_before_current_line + 2 * s->next;
    case CE_ISO_8859_1:
    case CE_ISO_8859_2:
    case CE_ISO_8859_3:
    case CE_ISO_8859_4:
    case CE_ISO_8859_5:
    case CE_ISO_8859_6:
    case CE_ISO_8859_7:
    case CE_ISO_8859_8:
    case CE_ISO_8859_9:
    case CE_unspecified_ascii_superset:
	return s->bytes_before_current_line + s->next;
    case CE_UTF_8:
	if(s->complicated_utf8_line)
	{
	    /* examine earlier chars in line to see how many bytes they used */
	    int i, c, n=0;
	    for(i = 0; i < s->next; i++)
	    {
		c = s->line[i];
		if(c <= 0x7f)
		    n += 1;
		else if(c <= 0x7ff)
		    n += 2;
		else if(c >= 0xd800 && c <= 0xdfff)
		    /* One of a surrogate pair, count 2 each */
		    n += 2;
		else if(c <= 0xffff)
		    n += 3;
		else if(c <= 0x1ffff)
		    n += 4;
		else if(c <= 0x3ffffff)
		    n += 5;
		else
		    n += 6;

	    }
	    return s->bytes_before_current_line + n;
	}
	else
	    return s->bytes_before_current_line + s->next;
    default:
	return -1;
    }
#endif
}

int SourceSeek(InputSource s, int offset)
{
    s->line_length = 0;
    s->next = 0;
    s->seen_eoe = 0;
    s->bytes_consumed = s->bytes_before_current_line = offset;
    s->nextin = s->insize = 0;
    /* XXX line number will be wrong! */
    s->line_number = -999999;
    return Fseek(s->file16, offset, SEEK_SET);
}

static int get_translated_line(InputSource s)
{
    /* This is a hack, pending some reorganisation */

    struct _FILE16 {
	void *handle;
	int handle2, handle3;
	/* we don't need the rest here */
    };

    Entity e = s->entity;
    Char *p;
    struct _FILE16 *f16 = (struct _FILE16 *)s->file16;


    if(e->type == ET_external)
	return get_translated_line1(s);

    if(!*(Char *)((char *)f16->handle + f16->handle2))
    {
	s->line_length = 0;
	return 0;
    }

    s->line = (Char *)((char *)f16->handle + f16->handle2);
    for(p=s->line; *p && *p != '\n'; p++)
	;
    if(*p)
	p++;
    f16->handle2 = (char *)p - (char *)f16->handle;
    s->line_length = p - s->line;

    s->bytes_before_current_line = f16->handle2;

    return 0;
}

static int get_translated_line1(InputSource s)
{
    unsigned int c;		/* can't use Char, it might be >0x10000 */
    unsigned char *inbuf = s->inbuf;
    int nextin = s->nextin, insize = s->insize;
    int startin = s->nextin;
    Char *outbuf = s->line;
    int outsize = s->line_alloc;
    int nextout = 0;
    int remaining = 0;
    int ignore_linefeed = s->line_end_was_cr;

#if CHAR_SIZE == 16

    int *to_unicode = 0;	/* initialize to shut gcc up */
    CharacterEncoding enc = s->entity->encoding;
    int more, i;
    s->complicated_utf8_line = 0;

    if(enc >= CE_ISO_8859_2 && enc <= CE_ISO_8859_9)
	to_unicode = iso_to_unicode[enc - CE_ISO_8859_2];

#endif

    s->line_end_was_cr = 0;
    s->bytes_before_current_line = s->bytes_consumed;

    while(1)
    {
	/* There are never more characters than bytes in the input */
	if(outsize < nextout + (insize - nextin))
	{
	    outsize = nextout + (insize - nextin);
	    outbuf = Realloc(outbuf, outsize * sizeof(Char));
	}

	while(nextin < insize)
	{
#if CHAR_SIZE == 8
	    c = inbuf[nextin++];
#else
	    switch(enc)
	    {
	    case CE_ISO_10646_UCS_2B:
	    case CE_UTF_16B:
		if(nextin+2 > insize)
		    goto more_bytes;
		c = (inbuf[nextin] << 8) + inbuf[nextin+1];
		nextin += 2;
		break;
	    case CE_ISO_10646_UCS_2L:
	    case CE_UTF_16L:
		if(nextin+2 > insize)
		    goto more_bytes;
		c = (inbuf[nextin+1] << 8) + inbuf[nextin];
		nextin += 2;
		break;
	    case CE_ISO_8859_1:
	    case CE_unspecified_ascii_superset:
		c = inbuf[nextin++];
		break;
	    case CE_ISO_8859_2:
	    case CE_ISO_8859_3:
	    case CE_ISO_8859_4:
	    case CE_ISO_8859_5:
	    case CE_ISO_8859_6:
	    case CE_ISO_8859_7:
	    case CE_ISO_8859_8:
	    case CE_ISO_8859_9:
		c = to_unicode[inbuf[nextin++]];
		if(c == (unsigned int)-1)
		  ERR3("Illegal %s character <0x%x> "
			    "at file offset %d\n",
			    CharacterEncodingName[enc], inbuf[nextin-1],
			    s->bytes_consumed + nextin - 1 - startin);
		break;
	    case CE_UTF_8:
		c = inbuf[nextin++];
		if(c <= 0x7f)
		    break;
		if(c <= 0xc0 || c >= 0xfe)
		{
		  ERR2("Illegal UTF-8 start byte <0x%x> "
			    "at file offset %d\n",
			    c, s->bytes_consumed + nextin - 1 - startin);
		    return -1;
		}
		if(c <= 0xdf)
		{
		    c &= 0x1f;
		    more = 1;
		}
		else if(c <= 0xef)
		{
		    c &= 0x0f;
		    more = 2;
		}
		else if(c <= 0xf7)
		{
		    c &= 0x07;
		    more = 3;
		}
		else if(c <= 0xfb)
		{
		    c &= 0x03;
		    more = 4;
		}
		else
		{
		    c &= 0x01;
		    more = 5;
		}
		if(nextin+more > insize)
		{
		    nextin--;
		    goto more_bytes;
		}
		s->complicated_utf8_line = 1;
		for(i=0; i<more; i++)
		    c = (c << 6) + (inbuf[nextin++] & 0x3f);
		break;
	    default:
	      ERR("read from entity with unsupported encoding!\n");
		return -1;
	    }

	    if(c > 0x110000 || (c < 0x10000 && !is_xml_legal(c)))
		if(!(enc == CE_UTF_16L || enc == CE_UTF_16B) ||
		   c < 0xd800 || c > 0xdfff)
		    /* We treat the surrogates as legal because we didn't
		       combine them when translating from UTF-16.  XXX */
		{
		  ERR2("Error: illegal character <0x%x> "
			    "immediately before file offset %d\n",
			    c, s->bytes_consumed + nextin - startin);
		    return -1;
		}
#endif
	    if(c == '\n' && ignore_linefeed)
	    {
		/* Ignore lf at start of line if last line ended with cr */
		ignore_linefeed = 0;
		s->bytes_before_current_line += (nextin - startin);
	    }		
	    else
	    {
		ignore_linefeed = 0;
		if(c == '\r')
		{
		    s->line_end_was_cr = 1;
		    c = '\n';
		}

#if CHAR_SIZE == 16
		if(c >= 0x10000)
		{
		    /* Use surrogates */
		    outbuf[nextout++] = ((c - 0x10000) >> 10) + 0xd800;
		    outbuf[nextout++] = ((c - 0x10000) & 0x3ff) + 0xdc00;
		}
		else
		    outbuf[nextout++] = c;
#else
		outbuf[nextout++] = c;
#endif

		if(c == '\n')
		{
		    s->nextin = nextin;
		    s->insize = insize;
		    s->bytes_consumed += (nextin - startin);
		    s->line = outbuf;
		    s->line_alloc = outsize;
		    s->line_length = nextout;
		    return 0;
		}
	    }
	}

#if CHAR_SIZE == 16
    more_bytes:
	/* Copy down any partial character */

	remaining = insize - nextin;
	for(i=0; i<remaining; i++)
	    inbuf[i] = inbuf[nextin + i];
#endif

	/* Get another block */

	s->bytes_consumed += (nextin - startin);

	insize = Readu(s->file16,
			inbuf+insize-nextin, sizeof(s->inbuf)-remaining);
	nextin = startin = 0;

	if(insize <= 0)
	{
		s->nextin = nextin;
		s->insize = 0;
		s->line = outbuf;
		s->line_alloc = outsize;
		s->line_length = nextout;
		return insize;
	}

	insize += remaining;
    }
}

void determine_character_encoding(InputSource s)
{
    Entity e = s->entity;
    int nread;
    unsigned char *b = (unsigned char *)s->inbuf;

    b[0] = b[1] = b[2] = b[3] = 0;

    while(s->insize < 4)
    {
	nread = Readu(s->file16, s->inbuf + s->insize, 4 - s->insize);
	if(nread == -1)
	    return;
	if(nread == 0)
	    break;
	s->insize += nread;
    }

#if 0
    if(b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == '<')
	e->encoding = CE_ISO_10646_UCS_4B;
    else if(b[0] == '<' && b[1] == 0 && b[2] == 0 && b[3] == 0)
	e->encoding = CE_ISO_10646_UCS_4L;
    else
#endif
    if(b[0] == 0xfe && b[1] == 0xff)
    {
	e->encoding = CE_UTF_16B;
	s->nextin = 2;
    }
    else if(b[0] == 0 && b[1] == '<' && b[2] == 0 && b[3] == '?')
	e->encoding = CE_UTF_16B;
    else if(b[0] == 0xff && b[1] == 0xfe)
    {
	e->encoding = CE_UTF_16L;
	s->nextin = 2;
    }
    else if(b[0] == '<' && b[1] == 0 && b[2] == '?' && b[3] == 0)
	e->encoding = CE_UTF_16L;
    else
    {
#if CHAR_SIZE == 8	
	e->encoding = CE_unspecified_ascii_superset;
#else
        e->encoding = CE_UTF_8;
#endif
    }
}

int get_with_fill(InputSource s)
{
    assert(!s->seen_eoe);

    if(get_translated_line(s) != 0)
    {
	/* It would be nice to pass this up to the parser, but we don't
	   know anything about parsers here! */
      ERR1("I/O error on stream <%s>, ignore further errors\n",
	      EntityDescription(s->entity));

	/* Restore old line and return EOE (is this the best thing to do?) */
	s->line_length = s->next;
	s->seen_eoe = 1;
	return XEOE;
    }

    if(s->line_length == 0)
    {
	/* Restore old line */
	s->line_length = s->next;
	s->seen_eoe = 1;
	return XEOE;
    }

    s->next = 0;

    if(s->not_read_yet)
	s->not_read_yet = 0;
    else
	s->line_number++;

    return s->line[s->next++];
}
