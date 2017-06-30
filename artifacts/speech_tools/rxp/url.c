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

#include "lt-defs.h"
#include "lt-memory.h"
#include "lt-errmsg.h"
#include "lt-comment.h"
#include "lt-safe.h"
#include "nsl-err.h"

#define Strerror() strErr()
#define Malloc salloc
#define Realloc srealloc
#define Free sfree
#define fopen stdsfopen

#else

#include "system.h"

#define LT_ERROR(err, format) fprintf(stderr, format)
#define LT_ERROR1(err, format, arg) fprintf(stderr, format, arg)
#define LT_ERROR2(err, format, arg1, arg2) fprintf(stderr, format, arg1, arg2)
#define LT_ERROR3(err, format, arg1, arg2, arg3) fprintf(stderr, format, arg1, arg2, arg3)
#define WARN(err, format) fprintf(stderr, format)
#define WARN1(err, format, arg) fprintf(stderr, format, arg)

#define Strerror() strerror(errno)

#ifdef MAXPATHLEN
#define CWDBS MAXPATHLEN+1
#else
#define CWDBS 1025
#endif

#define GETWD(buf) getcwd(buf,CWDBS)

#endif /* FOR_LT */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>		/* that's where strerror is.  really. */
#include <sys/types.h>

#ifdef WIN32
#include <direct.h>
#endif

#ifdef SOCKETS_IMPLEMENTED

#ifdef WIN32
#undef boolean
#include <winsock.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#endif

#include "string16.h"
#include "stdio16.h"
#include "url.h"

#ifdef HAVE_LIBZ
#include "zlib.h"
#ifdef macintosh
#include <fcntl.h>
#include <unix.h>
#endif
#endif

static FILE16 *http_open(const char *url, 
		       const char *host, int port, const char *path,
		       const char *type);
static FILE16 *file_open(const char *url,
		       const char *host, int port, const char *path,
		       const char *type);

static void parse_url(const char *url, 
		      char **scheme, char **host, int *port, char **path);

/* Mapping of scheme names to opening functions */

struct {
    char *scheme; 
    FILE16 *(*open)(const char *, const char *, int, const char *, const char *);
} schemes[] = {
    {(char *)"http", http_open},
    {(char *)"file", file_open},
};
#define NSCHEME (sizeof(schemes) / sizeof(schemes[0]))

/* Construct a default base URL, essentially file:`pwd`/ */

char *default_base_url(void)
{
    char buf[CWDBS];
    char *url;
    
    if(!GETWD(buf))
    {
	WARN(LEFILE, "Warning: can't get current directory for default base url\n");
	return strdup8("file:/");
    }


#ifdef WIN32

    /* DOS: translate C:\a\b to file:/C:/a/b/ */
    /* XXX should we escape anything? */
    {
    char *p;
    for(p=buf; *p; p++)
	if(*p == '\\')
	    *p = '/';
    }
    url = Malloc(6 + strlen(buf) + 2);
    sprintf(url, "file:/%s/", buf);

#else
#ifdef mac_filenames

    /* Mac: translate a:b to file:/a/b/ */
    /* XXX should escape spaces and slashes, at least */
    {
	char *p;
	for(p=buf; *p; p++)
	    if(*p == ':')
		*p = '/';
	/* Mac getcwd (always?) has a trailing separator, which we here bash */
	if(*--p == '/') 
	    *p = 0;
    }
    url = Malloc(6 + strlen(buf) + 2);
    sprintf(url, "file:/%s/", buf);

#else

    /* Unix: translate /a/b to file:/a/b/ */

    url = Malloc(5 + strlen(buf) + 2);
    sprintf(url, "file:%s/", buf);

#endif
#endif

    return url;
}

/* 
 * Merge a URL with a base URL if necessary.
 * The merged URL is returned.
 * The parts of the URL are returned in scheme, host, port and path
 * if these are non-null.
 * Caller should free the results.
 */

char *url_merge(const char *url, const char *base,
		       char **_scheme, char **_host, int *_port, char **_path)
{
    char *merged_scheme, *merged_host, *merged_path, *merged_url;
    char *scheme=0, *host=0, *path=0;
    char *base_scheme=0, *base_host=0, *base_path=0;
    char *default_base=0;
    int port, base_port, merged_port, i, j;
    char *p;
    
    /* First see if we have an absolute URL */

    parse_url(url, &scheme, &host, &port, &path);
    if(scheme && (host || *path == '/'))
    {
	merged_scheme = scheme;
	merged_host = host;
	merged_port = port;
	merged_path = path;
	merged_url = strdup8(url);
	goto ok;
    }

    /* Relative URL, so we need the base URL */

    if(!base)
	base = default_base = default_base_url();

    parse_url(base, &base_scheme, &base_host, &base_port, &base_path);
    if(base_scheme && (base_host || *base_path == '/'))
	;
    else 
    {
	LT_ERROR1(LEFILE, "Error: bad base URL <%s>\n", base);
	goto bad;
    }

    /* Determine merged path */

    if(path[0] == '/') 
    {
	/* not relative, use as-is */
	merged_path = path;
	path = 0;
    } 
    else 
    {
	/* relative, append to base path */

	merged_path = Malloc(strlen(base_path) + strlen(path) + 1);
	strcpy(merged_path, base_path);

	/* strip last component of base */

	for(i=strlen(merged_path)-1; i>=0 && merged_path[i] != '/'; i--)
	    merged_path[i] = '\0';

	/* append relative path */

	strcat(merged_path, path);
	
	/* Remove . and .. components from path */

	p = merged_path;
	for(i=0; p[i]; ) 
	{
	    assert(p[i] == '/');

	    /* find next segment */

	    for(j=i+1; p[j] && p[j] != '/'; j++)
		;

	    /* Do we have "." ? */

	    if(j - i == 2 && p[i+1] == '.')
	    {
		strcpy(&p[i+1], p[j] ? &p[j+1] : &p[j]);
		continue;
	    }

	    /* Do we have "<segment>/.."  with <segment> != ".." ? */

	    /* (We know we're not looking at "./" so we don't have to
	     * worry about "./..")
	     */

	    if(p[j] == '/' && p[j+1] == '.' && p[j+2] == '.' &&
	       (p[j+3] == '/' || p[j+3] == '\0') &&
	       (j - i != 3 || p[i+1] != '.' || p[i+2] != '.'))
	    {
		strcpy(&p[i+1], p[j+3] ? &p[j+4] : &p[j+3]);
		i = 0;		/* start again from beginning */
		continue;
	    }

	    /* move to next segment */

	    i = j;
	}
    }

    /* Check for deviant relative URLs like file:foo */

    if(scheme && !host && *path != '/')
    {
	if(strcmp(scheme, base_scheme) == 0)
	{
	    WARN1(LEFILE,
	  "Warning: relative URL <%s> contains scheme, contrary to RFC 1808\n",
		  url);
	}
	else
	{
	    LT_ERROR2(LEFILE,
	     "Error: relative URL <%s> has scheme different from base <%s>\n",
			 url, base);
	    goto bad;
	}
    }

    /* Return the parts and the whole thing */

    merged_scheme = base_scheme; if(scheme) Free(scheme);

    if(host)
    {
	merged_host = host; Free(base_host);
	merged_port = port;
    }
    else
    {
	merged_host = base_host;
	merged_port = base_port;
    }

    Free(path); Free(base_path);

    merged_url = Malloc(strlen(merged_scheme) + 1 + 
			(merged_host ? 2 + strlen(merged_host) + 10 : 0) +
			strlen(merged_path) + 1);
    if(merged_host) 
    {
	if(merged_port == -1)
	    sprintf(merged_url, "%s://%s%s", 
		    merged_scheme, merged_host, merged_path);
	else
	    sprintf(merged_url, "%s://%s:%d%s",
		    merged_scheme, merged_host, merged_port, merged_path);
    }
    else
	sprintf(merged_url, "%s:%s", merged_scheme, merged_path);

ok:
    Free(default_base);
    if(_scheme) *_scheme = merged_scheme; else Free(merged_scheme);
    if(_host) *_host = merged_host; else Free(merged_host);
    if(_port) *_port = merged_port;
    if(_path) *_path = merged_path; else Free(merged_path);

    return merged_url;

bad:
    Free(default_base);
    Free(scheme);
    Free(host);
    Free(path);
    Free(base_scheme);
    Free(base_host);
    Free(base_path);

    return NULL;
}

/* 
 * Open a stream to a URL.
 * url may be a relative URL, in which case it is merged with base,
 * which is typically the URL of the containing document.  If base
 * is null, file:`pwd`/ is used, which is the right thing to do for
 * filenames.  If base is "", there is no base URL and relative
 * URLs will fail.
 * If merged_url is non-null the resulting URL is stored in it.
 * If type begins "r", the URL is opened for reading, if "w" for
 * writing.  Writing is only supported for file URLs.
 * If the type begins "rl", the data will be copied to a temporary
 * file so that seeking is possible (NOT YET IMPLEMENTED).
 * Returns a FILE16 for success, NULL for failure.
 */

FILE16 *url_open(const char *url, const char *base, const char *type,
		 char **merged_url)
{
    char *scheme, *host, *path, *m_url;
    int port, i;
    FILE16 *f;
#ifdef HAVE_LIBZ
    int len, gzipped = 0;
#endif

    /* Determine the merged URL */

    if(!(m_url = url_merge(url, base, &scheme, &host, &port, &path)))
	return 0;

#ifdef HAVE_LIBZ
    len = strlen(m_url);
    if(len > 3 && strcmp8(m_url+len-3, ".gz") == 0)
	gzipped = 1;
#endif

    /*
    printf("<%s> <%s> <%d> <%s>\n", scheme, host ? host : "", port, path);
    printf("%s\n", m_url);
    */

    /* Pass to the appropriate opening function */

    for(i=0; i<NSCHEME; i++)
	if(strcmp(scheme, schemes[i].scheme) == 0) 
	{
	    f = schemes[i].open(m_url, host, port, path, type);
	
	    Free(scheme);
	    if(host)
		Free(host);
	    Free(path);

	    if(!f)
		return f;

#ifdef HAVE_LIBZ
	    if(gzipped)
	    {
		/* We have a gzip-compressed file which we hand to gzopen
		 * for further processing.
		 */
		 gzFile gfile;
		 FILE *file = GetFILE(f);

		 if(!f)
		 {
		     LT_ERROR1(LEFILE, 
				 "Can't attach gzip processor to URL \"%s\"\n",
				  m_url);
		     Free(m_url);
		     return 0;
		 }
#ifdef macintosh
		 gfile =gzdopen(dup(fileno(file)), *type == 'r' ? "rb" : "wb");
#else 	
		 gfile = gzdopen(dup(fileno(file)), type);
#endif
		 Fclose(f);
		 f = MakeFILE16FromGzip(gfile, type);
	    }
#endif
	    if(f && merged_url)
		*merged_url = m_url;
	    else
		Free(m_url);

	    return f;
	}

    /* Not implemented */

    LT_ERROR1(LEFILE, "Error: scheme \"%s\" not implemented\n", scheme);

    Free(scheme);
    if(host)
	Free(host);
    Free(path);
    Free(m_url);

    return 0;
}

/* Open an http URL */

static FILE16 *http_open(const char *url,
			 const char *host, int port, const char *path,
			 const char *type)
{
#ifndef SOCKETS_IMPLEMENTED
    LT_ERROR(NEUNSUP, 
		"http: URLs are not yet implemented on this platform\n");
    return 0;
#else
    FILE16 *f16;
    struct sockaddr_in addr;
    struct hostent *hostent;
    int s, server_major, server_minor, status, count, c;
    char reason[81];
#ifndef WIN32
    FILE *fin,*fout;
#else
    static int inited=0;
    int i;
    static char buf[1024];
    if (!inited) 
    {
	WORD version = MAKEWORD(1, 1);
	WSADATA wsaData;
	int err = WSAStartup(version, &wsaData);
	if (err)
	{
	    LT_ERROR(LEFILE, "Error: can't init HTTP interface\n");
	    return 0;
	}
	else if(LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
	{
	    LT_ERROR(LEFILE, "Error: wrong version of WINSOCK\n");
	    WSACleanup();
	    return 0;
	}
	inited = 1;
    }
#endif

    if(*type != 'r')
    {
	LT_ERROR1(LEFILE, "Error: can't open http URL \"%s\" for writing\n",
		     url);
	return 0;
    }

    if(!host)
    {
	LT_ERROR1(LEFILE, "Error: no host part in http URL \"%s\"\n", url);
	return 0;
    }

    /* Create the socket */

    s = socket(PF_INET, SOCK_STREAM, 0);
#ifdef WIN32
    if (s == INVALID_SOCKET) {
      LT_ERROR1(LEFILE, "Error: system call socket failed: %d\n",
		   WSAGetLastError());
    };
#else
    if(s == -1) {
	LT_ERROR1(LEFILE, "Error: system call socket failed: %s\n",
		     Strerror());
	return 0;
    };
#endif

    /* Find the server address */

    hostent = gethostbyname(host);
    if(!hostent)
    {
	LT_ERROR1(LEFILE,
		     "Error: can't find address for host in http URL \"%s\"\n",
		     url);
	return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    /* If we were really enthusiastic, we would try all the host's addresses */
    memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length);
    addr.sin_port = htons((u_short)(port == -1 ? 80 : port));

    /* Connect */

    if(connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
	LT_ERROR1(LEFILE, "Error: system call connect failed: %s\n",
		     Strerror());
	return 0;
    }

#ifndef WIN32
#ifdef macintosh
    fin = fdopen(s, "rb");
    setvbuf(fin, 0, _IONBF, 0);
    fout = fdopen(dup(s), "wb");
#else
    fin = fdopen(s, "r");
    setvbuf(fin, 0, _IONBF, 0);
    fout = fdopen(dup(s), "w");
#endif
#endif

    /* Send the request */

    /* 
     * Apparently on the Macintosh, \n might not be ASCII LF, so we'll
     * use numerics to be sure.
     */

#ifdef WIN32
    sprintf(buf, "GET %s HTTP/1.0\012\015Connection: close\012\015\012\015",
	    path);
    if (send(s,buf,strlen8(buf),0)==SOCKET_ERROR) {
      LT_ERROR1(LEFILE, "Error: system call socket failed: %d\n",
		   WSAGetLastError());
      /* XXX close the socket? */
      return 0;
    };      
#else
    fprintf(fout, "GET %s HTTP/1.0\012\015Connection: close\012\015\012\015",
	    path);

    /* We used to test for errors after doing fclose, but this seemed
       to produce spurious errors under Linux (RedHat 4.2), so now we
       do fflush and test after that. */

    fflush(fout);
    if(ferror(fout))
    {
	LT_ERROR1(LEWRTF, "Error: write to socket failed: %s\n",Strerror());
	fclose(fout);
	fclose(fin);
	return 0;
    }
    fclose(fout);
#endif

    /* Read the status line */
#ifdef WIN32
    for(i=0; i<sizeof(buf)-1; i++)
    {
	if(recv(s, &buf[i], 1, 0) != 1)
	    LT_ERROR1(LEFILE,
			 "Error: recv error from server for URL \"%s\"\n",
			 url);
	if(buf[i] == '\n')
	    break;
    }
    count=sscanf(buf, "HTTP/%d.%d %d %80[^\012]", 
		 &server_major, &server_minor, &status, reason);
#else    
    count=fscanf(fin, "HTTP/%d.%d %d %80[^\012]", 
		 &server_major, &server_minor, &status, reason);
#endif

    if(count != 4)
    {
	LT_ERROR3(LEFILE,
		     "Error: bad header from server for URL \"%s\"\n%d %s\n",
		     url, count, Strerror());
#ifndef WIN32
	fclose(fin);
#endif
	return 0;
    }

    if(status != 200)
    {
	/* We should handle 301 (redirection) but we don't */
	LT_ERROR3(LEFILE, "Error: can't retrieve \"%s\": %d %s\n",
		     url, status, reason);
#ifndef WIN32
	fclose(fin);
#endif
	return 0;
    }

    /* Skip other headers */

    count = 0;
#ifdef WIN32
    while(recv(s, buf, 1, 0) == 1 && (c = buf[0], 1) || (c = EOF, 0))
#else
    while((c = getc(fin)) != EOF)
#endif
    {
	if(c == '\012')
	    count++;
	else if(c != '\015')
	    count = 0;
	if(count == 2)
	    break;
    }

    if(c == EOF)
    {
	LT_ERROR1(LEFILE, "Error: EOF in headers retrieving \"%s\"\n", url);
#ifndef WIN32
	fclose(fin);
#endif
	return 0;
    }

#ifdef WIN32
    f16 = MakeFILE16FromWinsock(s, type);
#else
    f16 = MakeFILE16FromFILE(fin, type);
#endif

    SetCloseUnderlying(f16, 1);
    return f16;
#endif /* SOCKETS_IMPLEMENTED */
}

/* Open a file URL (easy, at least on unix) */

static FILE16 *file_open(const char *url,
			 const char *host, int port, const char *path, 
			 const char *type)
{
    FILE *f;
    FILE16 *f16;
    char *file;

    if(host && host[0])
	WARN1(LEFILE, "Warning: ignoring host part in file URL \"%s\"\n", url);

#ifdef WIN32

    /* DOS: translate /C:/a/b.c to C:\a\b.c */

    if(path[0] == '/' && path[1] && path[2] == ':')
	path++;

    file = strdup8(path);
    {
	char *p;
	for(p=file; *p; p++)
	    if(*p == '/')
		*p = '\\';
    }

#else
#ifdef mac_filenames

    /* Mac: translate /a/b.c to a:b.c */

    if(*path == '/')
	path++;

    file = strdup8(path);
    {
	char *p;
	for(p=file; *p; p++)
	    if(*p == '/')
		*p = ':';
    }
#else

    /* Unix: a path is a path is a path! */

    file = strdup8(path);

#endif
#endif

    /* XXX should undo any escapes */

    f = fopen(file, type);
    if(!f)
    {
	perror(file);
	Free(file);
	return 0;
    }

    Free(file);
    
    f16 = MakeFILE16FromFILE(f, type);
    SetCloseUnderlying(f16, 1);

    return f16;
}

static void parse_url(const char *url, 
		      char **scheme, char **host, int *port, char **path)
{
    char *p, *q;
    int warned = 0;

    *scheme = *host = *path = 0;
    *port = -1;

    /* Does it start with a scheme? */
    
    for(p = (char *)url; *p; p++)
	if(*p == ':' || *p == '/')
	    break;

    if(p > url && *p == ':')
    {
	*scheme = Malloc(p - url + 1);
	strncpy(*scheme, url, p - url);
	(*scheme)[p - url] = '\0';
	url = p+1;
    }

    /* Does it have a net_loc? */

    if(url[0] == '/' && url[1] == '/')
    {
	url += 2;

	for(p = (char *)url; *p; p++)
	    if(*p == '/')
		break;

	/* Does it have a port number? */

	for(q = p-1; q >= url; q--)
	    if(!isdigit((int)*q))
		break;

	if(q < p-1 && *q == ':')
	    *port = atoi(q+1);
	else
	    q = p;

	*host = Malloc(q - url + 1);
	strncpy(*host, url, q - url);
	(*host)[q - url] = '\0';
	url = p;
    }

    /* The rest is the path */

    if(*url)
	*path = strdup8(url);
    else
	*path = strdup8("/");

    /* Windoze users have a tendency to use backslashes instead of slashes */

    for(p=*path; *p; p++)
	if(*p == '\\')
	{
	    if(!warned)
	    {
		WARN1(LEFILE, "Warning: illegal backslashes in URL path \"%s\""
		              "replaced by slashes\n", url);
		warned = 1;
	    }

	    *p = '/';
	}
}

