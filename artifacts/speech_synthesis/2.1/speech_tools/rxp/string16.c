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
#ifdef FOR_LT

#include "lt-memory.h"

#define Malloc salloc
#define Realloc srealloc
#define Free sfree

#else

#include "system.h"

#endif

#include "charset.h"
#include "ctype16.h"
#include "string16.h"

int strcasecmp8(const char8 *s1, const char8 *s2)
{
    char8 c1, c2;

    while(1)
    {
	c1 = Toupper(*s1++);
	c2 = Toupper(*s2++);
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }
}

int strncasecmp8(const char8 *s1, const char8 *s2, size_t n)
{
    char8 c1, c2;

    while(n-- > 0)
    {
	c1 = Toupper(*s1++);
	c2 = Toupper(*s2++);
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }

    return 0;
}

char8 *strdup8(const char8 *s)
{
    char8 *buf;
    int len;

    len = strlen8(s);
    buf = Malloc(len + 1);
    if(!buf)
	return 0;

    strcpy8(buf, s);

    return buf;
}

/* 
 * NB these two functions return static storage, use Strdup or strdup8
 * if you want to keep the result.
 */

/* Convert a Latin-1 string to UTF-16 (easy!) */

char16 *char8tochar16(const char8 *s)
{
    static char16 *buf = 0;
    int i, len;

    len = strlen8(s);
    buf = Realloc(buf, (len + 1) * sizeof(char16));
    if(!buf)
	return 0;

    for(i=0; i<len; i++)
	buf[i] = s[i];
    buf[i] = 0;

    return buf;
}

/* Convert a UTF-16 string to Latin-1, replacing missing characters with Xs */

char8 *char16tochar8(const char16 *s)
{
    static char8 *buf = 0;
    int i, len;

    len = strlen16(s);
    buf = Realloc(buf, len + 1);
    if(!buf)
	return 0;

    for(i=0; i<len; i++)
	buf[i] = s[i] > 255 ? 'X' : s[i];
    buf[i] = 0;

    return buf;
}

char16 *strcpy16(char16 *s1, const char16 *s2)
{
    char16 *t = s1;

    while(*s2)
	*s1++ = *s2++;
    *s1 = 0;

    return t;
}

char16 *strncpy16(char16 *s1, const char16 *s2, size_t n)
{
    char16 *t = s1;

    while(n-- > 0 && *s2)
	*s1++ = *s2++;
    if(n > 0)
	*s1 = 0;

    return t;
}

char16 *strdup16(const char16 *s)
{
    char16 *buf;
    int len;

    len = strlen16(s);
    buf = Malloc((len + 1) * sizeof(char16));
    if(!buf)
	return 0;

    strcpy16(buf, s);

    return buf;
}

size_t strlen16(const char16 *s)
{
    int len = 0;

    while(*s++)
	len++;

    return len;
}

char16 *strchr16(const char16 *s, int c)
{
    for( ; *s; s++)
	if(*s == c)
	    return (char16 *)s;	/* Is const bogus or what? */

    return 0;
}

int strcmp16(const char16 *s1, const char16 *s2)
{
    char16 c1, c2;

    while(1)
    {
	c1 = *s1++;
	c2 = *s2++;
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }
}

int strncmp16(const char16 *s1, const char16 *s2, size_t n)
{
    char16 c1, c2;

    while(n-- > 0)
    {
	c1 = *s1++;
	c2 = *s2++;
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }

    return 0;
}

/* XXX only works for characters < 256 because Toupper does */

int strcasecmp16(const char16 *s1, const char16 *s2)
{
    char16 c1, c2;

    while(1)
    {
	c1 = Toupper(*s1++);
	c2 = Toupper(*s2++);
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }
}

int strncasecmp16(const char16 *s1, const char16 *s2, size_t n)
{
    char16 c1, c2;

    while(n-- > 0)
    {
	c1 = Toupper(*s1++);
	c2 = Toupper(*s2++);
	if(c1 == 0 && c2 == 0)
	    return 0;
	if(c1 == 0)
	    return -1;
	if(c2 == 0)
	    return 1;
	if(c1 < c2)
	    return -1;
	if(c1 > c2)
	    return 1;
    }

    return 0;
}

/* A very naive implementation */

char16 *strstr16(const char16 *s1, const char16 *s2)
{
    int len, first;

    first = s2[0];
    if(first == 0)
	return (char16 *)s1;

    len = strlen16(s2);

    while((s1 = strchr16(s1, first)))
    {
	if(strncmp16(s1, s2, len) == 0)
	    return (char16 *)s1;
	else
	    s1++;
    }

    return 0;
}

