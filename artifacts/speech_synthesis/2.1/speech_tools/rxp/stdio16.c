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
 * An implementation of printf that
 * - allows printing of 16-bit unicode characters and strings
 * - translates output to a specified character set
 *
 * "char8" is 8 bits and contains ISO-Latin-1 (or ASCII) values
 * "char16" is 16 bits and contains UTF-16 values
 * "Char" is char8 or char16 depending on whether CHAR_SIZE is 8 or 16
 *
 * Author: Richard Tobin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef FOR_LT

#include "lt-memory.h"
#include "nsl-err.h"

#define ERR(m) LT_ERROR(NECHAR,m)
#define ERR1(m,x) LT_ERROR1(NECHAR,m,x)
#define ERR2(m,x,y) LT_ERROR2(NECHAR,m,x,y)

#define Malloc salloc
#define Realloc srealloc
#define Free sfree

#else

#include "system.h"
#define WIN_IMP

#define ERR(m) fprintf(stderr,m)
#define ERR1(m,x) fprintf(stderr,m,x)
#define ERR2(m,x,y) fprintf(stderr,m,x,y)
#endif

#ifdef WIN32
#undef boolean
#include <winsock.h>
#include <fcntl.h>
#endif

#include "charset.h"
#include "string16.h"
#include "stdio16.h"

/* When we return -1 for non-io errors, we set errno to 0 to avoid confusion */
#include <errno.h>

#define BufferSize 4096

typedef int ReadProc(FILE16 *file, unsigned char *buf, int max_count);
typedef int WriteProc(FILE16 *file, const unsigned char *buf, int count);
typedef int SeekProc(FILE16 *file, long offset, int ptrname);
typedef int FlushProc(FILE16 *file);
typedef int CloseProc(FILE16 *file);

struct _FILE16 {
    void *handle;
    int handle2, handle3;
    ReadProc *read;
    WriteProc *write;
    SeekProc *seek;
    FlushProc *flush;
    CloseProc *close;
    int flags;
    CharacterEncoding enc;
    char16 save;
};

#define FILE16_read             0x01
#define FILE16_write            0x02
#define FILE16_close_underlying 0x04

static int FileRead(FILE16 *file, unsigned char *buf, int max_count);
static int FileWrite(FILE16 *file, const unsigned char *buf, int count);
static int FileSeek(FILE16 *file, long offset, int ptrname);
static int FileClose(FILE16 *file);
static int FileFlush(FILE16 *file);

static int StringRead(FILE16 *file, unsigned char *buf, int max_count);
static int StringWrite(FILE16 *file, const unsigned char *buf, int count);
static int StringSeek(FILE16 *file, long offset, int ptrname);
static int StringClose(FILE16 *file);
static int StringFlush(FILE16 *file);

#ifdef WIN32
#ifdef SOCKETS_IMPLEMENTED
static int WinsockRead(FILE16 *file, unsigned char *buf, int max_count);
static int WinsockWrite(FILE16 *file, const unsigned char *buf, int count);
static int WinsockSeek(FILE16 *file, long offset, int ptrname);
static int WinsockClose(FILE16 *file);
static int WinsockFlush(FILE16 *file);
#endif
#endif

#ifdef HAVE_LIBZ
static int GzipRead(FILE16 *file, unsigned char *buf, int max_count);
static int GzipWrite(FILE16 *file, const unsigned char *buf, int count);
static int GzipSeek(FILE16 *file, long offset, int ptrname);
static int GzipClose(FILE16 *file);
static int GzipFlush(FILE16 *file);
#endif

FILE16 *Stdin,  *Stdout, *Stderr;

void init_stdio16(void) {
    Stdin = MakeFILE16FromFILE(stdin, "r");
    SetFileEncoding(Stdin, CE_ISO_8859_1);
    Stdout = MakeFILE16FromFILE(stdout, "w");
    SetFileEncoding(Stdout, CE_ISO_8859_1);
    Stderr = MakeFILE16FromFILE(stderr, "w");
    SetFileEncoding(Stderr, CE_ISO_8859_1);
}

static int ConvertASCII(const char8 *buf, int count, FILE16 *file);
static int ConvertUTF16(const char16 *buf, int count, FILE16 *file);

/* Output an ASCII buffer in the specified encoding */

/* In fact, we don't translate the buffer at all if we are outputting
   an 8-bit encoding, and we treat it as Latin-1 is we are outputting
   a 16-bit encoding.  This means that all the various ASCII supersets
   will be passed through unaltered in the usual case, since we don't
   translate them on input either. */

static int ConvertASCII(const char8 *buf, int count, FILE16 *file)
{
    unsigned char outbuf[BufferSize*2];
    int i, j;

    switch(file->enc)
    {
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
	return file->write(file, (unsigned char *)buf, count);

    case CE_UTF_8:
	for(i=j=0; i<count; i++)
	{
	    unsigned char c = buf[i];
	    if(c < 128)
		outbuf[j++] = c;
	    else
	    {
		outbuf[j++] = 0xc0 + (c >> 6);
		outbuf[j++] = 0x80 + (c & 0x3f);
	    }
	}
	return file->write(file, outbuf, j);

    case CE_UTF_16B:
    case CE_ISO_10646_UCS_2B:
	for(i=j=0; i<count; i++)
	{
	    outbuf[j++] = 0;
	    outbuf[j++] = buf[i];
	}
	return file->write(file, outbuf, count*2);

    case CE_UTF_16L:
    case CE_ISO_10646_UCS_2L:
	for(i=j=0; i<count; i++)
	{
	    outbuf[j++] = buf[i];
	    outbuf[j++] = 0;
	}
	return file->write(file, outbuf, count*2);

    default:
      ERR2("Bad output character encoding %d (%s)\n",
		file->enc,
		file->enc < CE_enum_count ? CharacterEncodingName[file->enc] :
		                            "unknown");
	errno = 0;
	return -1;
    }
}

/* Output a UTF-16 buffer in the specified encoding */

static int ConvertUTF16(const char16 *buf, int count, FILE16 *file)
{
    /* size is max for UTF-8 with saved first half */
    unsigned char outbuf[BufferSize*3 + 1];
    int i, j, tablenum, max;
    char8 *from_unicode;
    char32 big;

    switch(file->enc)
    {
    case CE_ISO_8859_1:
    case CE_unspecified_ascii_superset:
	for(i=0; i<count; i++)
	{
	    if(buf[i] < 256)
		outbuf[i] = (unsigned char)buf[i];
	    else
		outbuf[i] = '?';
	}
	return file->write(file, outbuf, count);

    case CE_ISO_8859_2:
    case CE_ISO_8859_3:
    case CE_ISO_8859_4:
    case CE_ISO_8859_5:
    case CE_ISO_8859_6:
    case CE_ISO_8859_7:
    case CE_ISO_8859_8:
    case CE_ISO_8859_9:
	tablenum = (file->enc - CE_ISO_8859_2);
	max = iso_max_val[tablenum];
	from_unicode = unicode_to_iso[tablenum];
	for(i=0; i<count; i++)
	{
	    if(buf[i] <= max)
		outbuf[i] = (unsigned char)from_unicode[buf[i]];
	    else
		outbuf[i] = '?';
	}
	return file->write(file, outbuf, count);

    case CE_UTF_8:
	for(i=j=0; i<count; i++)
	{
	    if(buf[i] < 0x80)
		outbuf[j++] = (unsigned char)buf[i];
	    else if(buf[i] < 0x800)
	    {
		outbuf[j++] = 0xc0 + (buf[i] >> 6);
		outbuf[j++] = 0x80 + (buf[i] & 0x3f);
	    }
	    else if(buf[i] >= 0xd800 && buf[i] <= 0xdbff)
		file->save = buf[i];
	    else if(buf[i] >= 0xdc00 && buf[i] <= 0xdfff)
	    {
		big = 0x10000 + 
		    ((file->save - 0xd800) << 10) + (buf[i] - 0xdc00);
		outbuf[j++] = 0xf0 + (big >> 18);
		outbuf[j++] = 0x80 + ((big >> 12) & 0x3f);
		outbuf[j++] = 0x80 + ((big >> 6) & 0x3f);
		outbuf[j++] = 0x80 + (big & 0x3f);
	    }
	    else
	    {
		outbuf[j++] = 0xe0 + (buf[i] >> 12);
		outbuf[j++] = 0x80 + ((buf[i] >> 6) & 0x3f);
		outbuf[j++] = 0x80 + (buf[i] & 0x3f);
	    }
	}
	return file->write(file, outbuf, j);

    case CE_UTF_16B:
    case CE_ISO_10646_UCS_2B:
 	for(i=j=0; i<count; i++)
	{
	    outbuf[j++] = (buf[i] >> 8);
	    outbuf[j++] = (buf[i] & 0xff);

	}
	return file->write(file, outbuf, count*2);

    case CE_UTF_16L:
    case CE_ISO_10646_UCS_2L:
 	for(i=j=0; i<count; i++)
	{
	    outbuf[j++] = (buf[i] & 0xff);
	    outbuf[j++] = (buf[i] >> 8);

	}
	return file->write(file, outbuf, count*2);

    default:
      ERR2("Bad output character encoding %d (%s)\n",
		file->enc,
		file->enc < CE_enum_count ? CharacterEncodingName[file->enc] :
		                            "unknown");
	errno = 0;
	return -1;
    }
}

int Readu(FILE16 *file, unsigned char *buf, int max_count)
{
    return file->read(file, buf, max_count);
}

int Writeu(FILE16 *file, unsigned char *buf, int count)
{
    return file->write(file, buf, count);
}

int Fclose(FILE16 *file)
{
    int ret = 0;

    ret = file->close(file);
    Free(file);
    
    return ret;
}

int Fseek(FILE16 *file, long offset, int ptrname)
{
    return file->seek(file, offset, ptrname);
}

int Fflush(FILE16 *file)
{
    return file->flush(file);
}

FILE *GetFILE(FILE16 *file)
{
    if(file->read == FileRead)
	return (FILE *)file->handle;
    else
	return 0;
}

void SetCloseUnderlying(FILE16 *file, int cu)
{
    if(cu)
	file->flags |= FILE16_close_underlying;
    else
	file->flags &= ~FILE16_close_underlying;
}

void SetFileEncoding(FILE16 *file, CharacterEncoding encoding)
{
    file->enc = encoding;
}

CharacterEncoding GetFileEncoding(FILE16 *file)
{
    return file->enc;
}

int Fprintf(FILE16 *file, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return Vfprintf(file, format, args);
}

int Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return Vfprintf(Stdout, format, args);
}

int Sprintf(void *buf, CharacterEncoding enc, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return Vsprintf(buf, enc, format, args);
}

int Vprintf(const char *format, va_list args)
{
    return Vfprintf(Stdout, format, args);
}

int Vsprintf(void *buf, CharacterEncoding enc, const char *format, 
	     va_list args)
{
    int nchars;
    FILE16 file = {0, 0, -1, StringRead, StringWrite, StringSeek, StringFlush, StringClose, FILE16_write};

    file.handle = buf;
    file.enc = enc;

    nchars = Vfprintf(&file, format, args);
    file.close(&file);		/* Fclose would try to free it */

    return nchars;
}

#define put(x) {nchars++; if(count == sizeof(buf)) {if(ConvertASCII(buf, count, file) == -1) return -1; count = 0;} buf[count++] = x;}

int Vfprintf(FILE16 *file, const char *format, va_list args)
{
    char8 buf[BufferSize];
    int count = 0;
    int c, i, n, width, prec;
    char fmt[200];
    char8 val[200];
    const char8 *start;
    const char8 *p;
    const char16 *q;
    char16 cbuf[1];
    int mflag, pflag, sflag, hflag, zflag;
    int l, h, L;
    int nchars = 0;

    while((c = *format++))
    {
	if(c != '%')
	{
	    put(c);
	    continue;
	}

	start = format-1;
	width = 0;
	prec = -1;
	mflag=0, pflag=0, sflag=0, hflag=0, zflag=0;
	l=0, h=0, L=0;

	while(1)
	{
	    switch(c = *format++)
	    {
	    case '-':
		mflag = 1;
		break;
	    case '+':
		pflag = 1;
		break;
	    case ' ':
		sflag = 1;
		break;
	    case '#':
		hflag = 1;
		break;
	    case '0':
		zflag = 1;
		break;
	    default:
		goto flags_done;
	    }
	}
    flags_done:

	if(c == '*')
	{
	    width = va_arg(args, int);
	    c = *format++;
	}
	else if(c >= '0' && c <= '9')
	{
	    width = c - '0';
	    while((c = *format++) >= '0' && c <= '9')
		width = width * 10 + c - '0';
	}

	if(c == '.')
	{
	    c = *format++;
	    if(c == '*')
	    {
		prec = va_arg(args, int);
		c = *format++;
	    }
	    else if(c >= '0' && c <= '9')
	    {
		prec = c - '0';
		while((c = *format++) >= '0' && c <= '9')
		    prec = prec * 10 + c - '0';
	    }
	    else
		prec = 0;
	}

	switch(c)
	{
	case 'l':
	    l = 1;
	    c = *format++;
	    break;
	case 'h':
	    h = 1;
	    c = *format++;
	    break;
#ifdef HAVE_LONG_DOUBLE
	case 'L':
	    L = 1;
	    c = *format++;
	    break;
#endif
	}

	if(format - start + 1 > sizeof(fmt))
	{
	  ERR("Printf: format specifier too long");
	    errno = 0;
	    return -1;
	}

	strncpy(fmt, start, format - start);
	fmt[format - start] = '\0';

	/* XXX should check it fits in val */

	switch(c)
	{
	case 'n':
	    *va_arg(args, int *) = nchars;
	    break;
	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	    if(h)
		sprintf(val, fmt, va_arg(args, int)); /* promoted to int */
	    else if(l)
		sprintf(val, fmt, va_arg(args, long));
	    else
		sprintf(val, fmt, va_arg(args, int));
	    for(p=val; *p; p++)
		put(*p);
	    break;
	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
#ifdef HAVE_LONG_DOUBLE
	    if(L)
		sprintf(val, fmt, va_arg(args, long double));
	    else
#endif
		sprintf(val, fmt, va_arg(args, double));
	    for(p=val; *p; p++)
		put(*p);
	    break;
	case 'c':
#if CHAR_SIZE == 16
	    if(ConvertASCII(buf, count, file) == -1)
		return -1;
	    count = 0;
	    cbuf[0] = va_arg(args, int);
	    if(ConvertUTF16(cbuf, 1, file) == -1)
		return -1;
#else
            (void)cbuf;
            put(va_arg(args, int));
#endif
	    break;
	case 'p':
	    sprintf(val, fmt, va_arg(args, void *));
	    for(p=val; *p; p++)
		put(*p);
	    break;
	case '%':
	    put('%');
	    break;
	case 's':
	    if(l)
	    {
		static char16 sNULL[] = {'(','N','U','L','L',')',0};
#if CHAR_SIZE == 16
	    string:
#endif
		q = va_arg(args, char16 *);
		if(!q)
		    q = sNULL;
		n = strlen16(q);
		if(prec >= 0 && n > prec)
		    n = prec;
		if(n < width && !mflag)
		    for(i=width-n; i>0; i--)
			put(' ');
		if(ConvertASCII(buf, count, file) == -1)
		    return -1;
		count = 0;
		nchars += n;
		while(n > 0)
		{
		    /* ConvertUTF16 can only handle <= BufferSize chars */
		    if(ConvertUTF16(q, n > BufferSize ? BufferSize : n, file) == -1)
			return -1;
		    n -= BufferSize;
		    q += BufferSize;
		}
	    }
	    else
	    {
#if CHAR_SIZE == 8
	    string:
#endif
		p = va_arg(args, char8 *);
		if(!p)
		    p = "(null)";
		n = strlen(p);
		if(prec >= 0 && n > prec)
		    n = prec;
		if(n < width && !mflag)
		    for(i=width-n; i>0; i--)
			put(' ');
		for(i=0; i<n; i++)
		    put(p[i]);
	    }
	    if(n < width && mflag)
		for(i=width-n; i>0; i--)
		    put(' ');
	    break;
	case 'S':
	    goto string;
	default:
	  ERR1("unknown format character %c\n", c);
	    errno = 0;
	    return -1;
	}
    }

    if(count > 0)
	if(ConvertASCII(buf, count, file) == -1)
	    return -1;

    return nchars;
}

static FILE16 *MakeFILE16(const char *type)
{
    FILE16 *file;

    if(!(file = Malloc(sizeof(*file))))
	return 0;

    file->flags = 0;
    if(*type == 'r')
	file->flags |= FILE16_read;
    else
	file->flags |= FILE16_write;

    file->enc = InternalCharacterEncoding;

    return file;
}

FILE16 *MakeFILE16FromFILE(FILE *f, const char *type)
{
    FILE16 *file;

    if(!(file = MakeFILE16(type)))
	return 0;

    file->read = FileRead;
    file->write = FileWrite;
    file->seek = FileSeek;
    file->close = FileClose;
    file->flush = FileFlush;
    file->handle = f;

    return file;
}

static int FileRead(FILE16 *file, unsigned char *buf, int max_count)
{
    FILE *f = file->handle;
    int count = 0;

    count = fread(buf, 1, max_count, f);

    return ferror(f) ? -1 : count;
}

static int FileWrite(FILE16 *file, const unsigned char *buf, int count)
{
    FILE *f = file->handle;

    if(count == 0)
	return 0;
    return fwrite(buf, 1, count, f) == 0 ? -1 : 0;
}

static int FileSeek(FILE16 *file, long offset, int ptrname)
{
    FILE *f = file->handle;

    return fseek(f, offset, ptrname);
}

static int FileClose(FILE16 *file)
{
    FILE *f = file->handle;

    return (file->flags & FILE16_close_underlying) ? fclose(f) : 0;
}

static int FileFlush(FILE16 *file)
{
    FILE *f = file->handle;

    return fflush(f);
}

FILE16 *MakeFILE16FromString(void *buf, long size, const char *type)
{
    FILE16 *file;

    if(!(file = MakeFILE16(type)))
	return 0;

    file->read = StringRead;
    file->write = StringWrite;
    file->seek = StringSeek;
    file->close = StringClose;
    file->flush = StringFlush;

    file->handle = buf;
    file->handle2 = 0;
    file->handle3 = size;

    return file;
}

static int StringRead(FILE16 *file, unsigned char *buf, int max_count)
{
    char *p = (char *)file->handle + file->handle2;

    if(file->handle3 >= 0 && file->handle2 + max_count > file->handle3)
	max_count = file->handle3 - file->handle2;

    if(max_count <= 0)
	return 0;

    memcpy(buf, p, max_count);
    file->handle2 += max_count;

    return max_count;
}

static int StringWrite(FILE16 *file, const unsigned char *buf, int count)
{
    char *p = (char *)file->handle + file->handle2;

    if(file->handle3 >= 0 && file->handle2 + count > file->handle3)
	return -1;

    memcpy(p, buf, count);
    file->handle2 += count;

    return 0;
}

static int StringSeek(FILE16 *file, long offset, int ptrname)
{
    switch(ptrname)
    {
    case SEEK_CUR:
	offset = file->handle2 + offset;
	break;
    case SEEK_END:
	if(file->handle3 < 0)
	    return -1;
	offset = file->handle3 + offset;
	break;
    }

    if(file->handle3 >= 0 && offset > file->handle3)
	return -1;

    file->handle2 = offset;

    return 0;
}

static int StringClose(FILE16 *file)
{
    static char8 null = 0;

    if(file->flags & FILE16_write)
	ConvertASCII(&null, 1, file); /* null terminate */

    if(file->flags & FILE16_close_underlying)
	Free((char *)file->handle);

    return 0;
}

static int StringFlush(FILE16 *file)
{
    return 0;
}


#ifdef WIN32
#ifdef SOCKETS_IMPLEMENTED

FILE16 *MakeFILE16FromWinsock(int sock, const char *type)
{
    FILE16 *file;

    if(!(file = MakeFILE16(type)))
	return 0;

    file->read = WinsockRead;
    file->write = WinsockWrite;
    file->seek = WinsockSeek;
    file->close = WinsockClose;
    file->flush = WinsockFlush;
    file->handle2 = sock;

    return file;
}

static int WinsockRead(FILE16 *file, unsigned char *buf, int max_count)
{
    int f = (int)file->handle2;
    int count;

    /* "Relax" said the nightman, we are programmed to recv() */
    count = recv(f, buf, max_count, 0);

    return count;
}

static int WinsockWrite(FILE16 *file, const unsigned char *buf, int count)
{
    /* Not yet implemented */

    return -1;
}

static int WinsockSeek(FILE16 *file, long offset, int ptrname)
{
    return -1;
}

static int WinsockClose(FILE16 *file)
{
    /* What should happen here? XXX */

    return 0;
}

static int WinsockFlush(FILE16 *file)
{
    return 0;
}

#endif
#endif

#ifdef HAVE_LIBZ

FILE16 *MakeFILE16FromGzip(gzFile f, const char *type)
{
    FILE16 *file;

    if(!(file = MakeFILE16(type)))
	return 0;

    file->read = GzipRead;
    file->write = GzipWrite;
    file->seek = GzipSeek;
    file->close = GzipClose;
    file->flush = GzipFlush;
    file->handle = (void *)f;

    return file;
}

static int GzipRead(FILE16 *file, unsigned char *buf, int max_count)
{
    gzFile f = (gzFile)file->handle;
    int count = 0;
    int gzerr;
    const char *errorString;

    count = gzread(f, buf, max_count);

    errorString = gzerror(f, &gzerr);
    if(gzerr != 0 && gzerr != Z_STREAM_END)
	return -1;
    
    return count;
}

static int GzipWrite(FILE16 *file, const unsigned char *buf, int count)
{
    gzFile f = (gzFile)file->handle;
    int gzerr;
    const char *errorString;

    count = gzwrite(f, (char *)buf, count);

    errorString = gzerror(f, &gzerr);
    if(gzerr != 0 && gzerr != Z_STREAM_END)
	return -1;
    
    return count;
}

static int GzipSeek(FILE16 *file, long offset, int ptrname)
{
    return -1;
}

static int GzipClose(FILE16 *file)
{
    gzFile f = (gzFile)file->handle;

    return (file->flags & FILE16_close_underlying) ? gzclose(f) : 0;
}

static int GzipFlush(FILE16 *file)
{
    return 0;
}

#endif

#ifdef test

int main(int argc, char **argv)
{
    short s=3;
    int n, c;
    char16 S[] = {'w', 'o', 'r', 'l', 'd', ' ', '£' & 0xff, 0xd841, 0xdc42, 0};

    n=Printf(argv[1], s, 98765432, &c, 5.3, 3.2L, "÷hello", S);
    printf("\nreturned %d, c=%d\n", n, c);
    n=Printf(argv[1], s, 98765432, &c, 5.3, 3.2L, "÷hello", S);
    printf("\nreturned %d, c=%d\n", n, c);
    n=Printf(argv[1], s, 98765432, &c, 5.3, 3.2L, "÷hello", S);
    printf("\nreturned %d, c=%d\n", n, c);
    n=Printf(argv[1], s, 98765432, &c, 5.3, 3.2L, "÷hello", S);
    printf("\nreturned %d, c=%d\n", n, c);

    return 0;
}

#endif

