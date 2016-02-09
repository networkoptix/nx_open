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
/* 	$Id: xmlparser.c,v 1.3 2004/05/04 00:00:17 awb Exp $	 */

#ifndef lint
static char vcid[] = "$Id: xmlparser.c,v 1.3 2004/05/04 00:00:17 awb Exp $";
#endif /* lint */

/* 
 * XML (and nSGML) parser.
 * Author: Richard Tobin.
 */

#include <stdarg.h>
#include <stdlib.h>

#ifdef FOR_LT

#include "lt-memory.h"
#include "nsllib.h"

#define Malloc salloc
#define Realloc srealloc
#define Free sfree

#else

#include "system.h"

#endif

#include "charset.h"
#include "string16.h"
#include "ctype16.h"
#include "dtd.h"
#include "input.h"
#include "stdio16.h"
#include "xmlparser.h"

static int transcribe(Parser p, int back, int count);
static void pop_while_at_eoe(Parser p);
static void maybe_uppercase(Parser p, Char *s);
static void maybe_uppercase_name(Parser p);
static int str_maybecase_cmp8(Parser p, const char8 *a, const char8 *b);
static int is_ascii_alpha(int c);
static int is_ascii_digit(int c);
static int parse_external_id(Parser p, int required, 
			     char8 **publicid, char8 **systemid, 
			     int preq, int sreq);
static int parse_conditional(Parser p);
static int parse_notation_decl(Parser p);
static int parse_entity_decl(Parser p, Entity ent, int line, int chpos);
static int parse_attlist_decl(Parser p);
static int parse_element_decl(Parser p);
static ContentParticle parse_cp(Parser p);
static ContentParticle parse_choice_or_seq(Parser p);
static ContentParticle parse_choice_or_seq_1(Parser p, int nchildren,char sep);
static int check_content_decl(Parser p, ContentParticle cp);
static int check_content_decl_1(Parser p, ContentParticle cp);
static Char *stringify_cp(ContentParticle cp);
static void print_cp(ContentParticle cp, FILE16 *f);
static int size_cp(ContentParticle cp);
void FreeContentParticle(ContentParticle cp);
static int parse_reference(Parser p, int pe, int expand, int allow_external);
static int parse_character_reference(Parser p, int expand);
static const char8 *escape(int c);
static int parse_name(Parser p, const char8 *where);
static int parse_nmtoken(Parser p, const char8 *where);
static int looking_at(Parser p, const char8 *string);
static void clear_xbit(XBit xbit);
static int expect(Parser p, int expected, const char8 *where);
static int expect_dtd_whitespace(Parser p, const char8 *where);
static void skip_whitespace(InputSource s);
static int skip_dtd_whitespace(Parser p, int allow_pe);
static int parse_cdata(Parser p);
static int process_nsl_decl(Parser p);
static int process_xml_decl(Parser p);
static int parse_dtd(Parser p);
static int read_markupdecls(Parser p);
static int error(Parser p, const char8 *format, ...);
static void warn(Parser p, const char8 *format, ...);
static void verror(XBit bit, const char8 *format, va_list args);
enum literal_type {LT_cdata_attr, LT_tok_attr, LT_plain, LT_entity};
static int parse_string(Parser p, const char8 *where, enum literal_type type);
static int parse_pi(Parser p);
static int parse_comment(Parser p, int skip);
static int parse_pcdata(Parser p);
static int parse_starttag(Parser p);
static int parse_attribute(Parser p);
static int parse_endtag(Parser p);
static int parse_markup(Parser p);
static int parse(Parser p);
static int parse_markupdecl(Parser p);

#define require(x)  if(x >= 0) {} else return -1
#define require0(x) if(x >= 0) {} else return 0

#define Consume(buf) (buf = 0, buf##size = 0)
#define ExpandBuf(buf, sz) \
    if(buf##size >= (sz)+1) {} else if((buf = Realloc(buf, (buf##size = sz + 1) * sizeof(Char)))) {} else return error(p, "System error")

#define CopyName(n) if((n = Malloc((p->namelen + 1)*sizeof(Char)))) {memcpy(n, p->name, p->namelen * sizeof(Char)); n[p->namelen] = 0;} else return error(p, "System error");

#define CopyName0(n) if((n = Malloc((p->namelen + 1)*sizeof(Char)))) {memcpy(n, p->name, p->namelen * sizeof(Char)); n[p->namelen] = 0;} else {error(p, "System error"); return 0;}

const char8 *XBitTypeName[XBIT_enum_count] = {
    "dtd",
    "start",
    "empty",
    "end",
    "eof",
    "pcdata",
    "pi",
    "comment",
    "cdsect",
    "xml",
    "error",
    "warning",
    "none"
};

static Entity xml_builtin_entity;
static Entity xml_predefined_entities;

int ParserInit(void)
{
    static int initialised = 0;
    Entity e, f;
    int i;
    static const Char lt[] = {'l','t',0}, ltval[] = {'&','#','6','0',';',0};
    static const Char gt[] = {'g','t',0}, gtval[] = {'&','#','6','2',';',0};
    static const Char amp[] = {'a','m','p',0},
		      ampval[] = {'&','#','3','8',';',0};
    static const Char apos[] = {'a','p','o','s',0}, aposval[] = {'\'',0};
    static const Char quot[] = {'q','u','o','t',0}, quotval[] = {'"',0};
    static const Char *builtins[5][2] = {
	{lt, ltval}, {gt, gtval}, {amp, ampval},
	{apos, aposval}, {quot, quotval}
    };
    (void)vcid;

    if(initialised)
	return 0;
    initialised = 1;

    init_charset();
    init_ctype16();
    init_stdio16();

    for(i=0, f=0; i<5; i++, f=e)
    {
	e = NewInternalEntity(builtins[i][0], builtins[i][1],
			      xml_builtin_entity, 0, 0, 0);
	if(!e)
	    return -1;
	e->next = f;
    }

    xml_predefined_entities = e;

    return 0;
}

static void skip_whitespace(InputSource s)
{
    int c;

    while((c = get(s)) != XEOE && is_xml_whitespace(c))
	;
    unget(s);
}

/* 
 * Skip whitespace and (optionally) the start and end of PEs.  Return 1 if
 * there actually *was* some whitespace or a PE start/end, -1 if
 * an error occurred, 0 otherwise.
 */

static int skip_dtd_whitespace(Parser p, int allow_pe)
{
    int c;
    int got_some = 0;
    InputSource s = p->source;

    while(1)
    {
	c = get(s);

	if(c == XEOE)
	{
	    got_some = 1;
	    if(s->parent)
	    {
		if(!allow_pe)
		    return error(p,
				 "PE end not allowed here in internal subset");
		if(s->entity->type == ET_external)
		    p->external_pe_depth--;
		ParserPop(p);
		s = p->source;
	    }
	    else
	    {
		unget(s);	/* leave the final EOE waiting to be read */
		return got_some;
	    }
	}
	else if(is_xml_whitespace(c))
	{
	    got_some = 1;
	}
	else if(c == '%')
	{
	    /* this complication is needed for <!ENTITY % ...
	       otherwise we could just assume it was a PE reference. */

	    c = get(s); unget(s);
	    if(c != XEOE && is_xml_namestart(c))
	    {
		if(!allow_pe)
		{
		    unget(s);	/* For error position */
		    return error(p, 
				 "PE ref not allowed here in internal subset");
		}
		require(parse_reference(p, 1, 1, 1));
		s = p->source;
		if(s->entity->type == ET_external)
		    p->external_pe_depth++;
		got_some = 1;
	    }
	    else
	    {
		unget(s);
		return got_some;
	    }
	}
	else
	{
	    unget(s);
	    return got_some;
	}
    }
}

static int expect(Parser p, int expected, const char8 *where)
{
    int c;
    InputSource s = p->source;

    c = get(s);
    if(c != expected)
    {
	unget(s);		/* For error position */
	return error(p, "Expected %s %s, but got %s",
		     escape(expected), where, escape(c));
    }

    return 0;
}

/* 
 * Expects whitespace or the start or end of a PE.
 */

static int expect_dtd_whitespace(Parser p, const char8 *where)
{
    int r = skip_dtd_whitespace(p, p->external_pe_depth > 0);

    if(r < 0)
	return -1;

    if(r == 0)
	return error(p, "Expected whitespace %s", where);

    return 0;
}

static void clear_xbit(XBit xbit)
{
    xbit->type = XBIT_none;
    xbit->s1 = xbit->s2 = 0;
    xbit->S1 = xbit->S2 = 0;
    xbit->attributes = 0;
    xbit->element_definition = 0;
}

void FreeXBit(XBit xbit)
{
    Attribute a, b;

    if(xbit->S1) Free(xbit->S1);
    if(xbit->S2) Free(xbit->S2);
    if(xbit->type != XBIT_error && xbit->type != XBIT_warning && xbit->s1)
	Free(xbit->s1);
    if(xbit->s2) Free(xbit->s2);
    for(a = xbit->attributes; a; a = b)
    {
	b = a->next;
	if(a->value) Free(a->value);
	Free(a);
    }
    clear_xbit(xbit);
}

/* 
 * Returns 1 if the input matches string (and consume the input).
 * Otherwise returns 0 and leaves the input stream where it was.
 * Case-sensitivity depends on the CaseInsensitive flag.
 * A space character at end of string matches any (non-zero) amount of
 * whitespace; space are treated literally elsewhere.
 * Never reads beyond an end-of-line, except to consume
 * extra whitespace when the last character of string is a space.
 * Never reads beyond end-of-entity.
 */

static int looking_at(Parser p, const char8 *string)
{
    InputSource s = p->source;
    int c, d;
    int save = s->next;

    for(c = *string++; c; c = *string++)
    {
	if(at_eol(s))
	    goto fail;		/* We would go over a line end */

	d = get(s);

	if(c == ' ' && *string == 0)
	{
	    if(d == XEOE || !is_xml_whitespace(d))
		goto fail;
	    skip_whitespace(s);
	}
	else
	    if((ParserGetFlag(p, CaseInsensitive) &&
		Toupper(d) != Toupper(c)) ||
	       (!ParserGetFlag(p, CaseInsensitive) && d != c))
		goto fail;
    }

    return 1;

fail:
    s->next = save;
    return 0;
}

static int parse_name(Parser p, const char8 *where)
{
    InputSource s = p->source;
    int c, i;

    c = get(s);
    if(c == XEOE || !is_xml_namestart(c))
    {
	unget(s);		/* For error position */
	error(p, "Expected name, but got %s %s", escape(c), where);
	return -1;
    }
    i = 1;

    while(c = get(s), (c != XEOE && is_xml_namechar(c)))
	i++;
    unget(s);

    p->name = s->line + s->next - i;
    p->namelen = i;

    return 0;
}

static int parse_nmtoken(Parser p, const char8 *where)
{
    InputSource s = p->source;
    int c, i=0;

    while(c = get(s), (c !=XEOE && is_xml_namechar(c)))
	i++;
    unget(s);

    if(i == 0)
	return error(p, "Expected nmtoken value, but got %s %s", 
		     escape(c), where);

    p->name = s->line + s->next - i;
    p->namelen = i;

    return 0;
}

/* Escape a character for printing n an error message.  
   NB returns 5 static storage buffers in rotation. */

static const char8 *escape(int c)
{
    static char8 buf[5][15];
    static int bufnum=-1;

#if CHAR_SIZE == 8
    if(c != XEOE)
	c &= 0xff;
#endif

    bufnum = (bufnum + 1) % 5;

    if(c == XEOE)
	return "<EOE>";
    else if(c >= 33 && c <= 126)
	sprintf(buf[bufnum], "%c", c);
    else if(c == ' ')
	sprintf(buf[bufnum], "<space>");
    else
	sprintf(buf[bufnum], "<0x%x>", c);
    
    return buf[bufnum];
}

Parser NewParser(void)
{
    Parser p;

    if(ParserInit() == -1)
	return 0;

    p = Malloc(sizeof(*p));
    if(!p)
	return 0;
    p->state = PS_prolog1;
    p->document_entity = 0;	/* Set at first ParserPush */
    p->have_dtd = 0;
    p->standalone = SDD_unspecified;
    p->flags = 0;
    p->source = 0;
    clear_xbit(&p->xbit);
#ifndef FOR_LT
    p->xbit.nchildren = 0;	/* These three should never be changed */
    p->xbit.children = 0;
    p->xbit.parent = 0;
#endif
    p->pbufsize = p->pbufnext = 0;
    p->pbuf = 0;
    p->peeked = 0;
    p->dtd = NewDtd();
    p->dtd_callback = p->warning_callback = 0;
    p->entity_opener = 0;
    p->callback_arg = 0;
    p->external_pe_depth = 0;

    p->element_stack = 0;
    p->element_stack_alloc = 0;
    p->element_depth = 0;

    ParserSetFlag(p, XMLPiEnd, 1);
    ParserSetFlag(p, XMLEmptyTagEnd, 1);
    ParserSetFlag(p, XMLPredefinedEntities, 1);
    ParserSetFlag(p, XMLExternalIDs, 1);
    ParserSetFlag(p, XMLMiscWFErrors, 1);
    ParserSetFlag(p, ErrorOnUnquotedAttributeValues, 1);
    ParserSetFlag(p, XMLLessThan, 1);
    ParserSetFlag(p, IgnoreEntities, 0);
    ParserSetFlag(p, ExpandGeneralEntities, 1);
    ParserSetFlag(p, ExpandCharacterEntities, 1);
    ParserSetFlag(p, NormaliseAttributeValues, 1);
    ParserSetFlag(p, WarnOnUndefinedElements, 1);
    ParserSetFlag(p, WarnOnUndefinedAttributes, 1);
    ParserSetFlag(p, WarnOnRedefinitions, 1);
    ParserSetFlag(p, TrustSDD, 1);
    ParserSetFlag(p, ReturnComments, 1);
    ParserSetFlag(p, CheckEndTagsMatch, 1);

    return p;
}

void FreeParser(Parser p)
{
    while (p->source)
	ParserPop(p);		/* Will close file */

    Free(p->pbuf);
    Free(p->element_stack);
    Free(p);
}

InputSource ParserRootSource(Parser p)
{
    InputSource s;

    for(s=p->source; s && s->parent; s = s->parent)
	;

    return s;
}

Entity ParserRootEntity(Parser p)
{
    return ParserRootSource(p)->entity;
}

void ParserSetCallbackArg(Parser p, void *arg)
{
    p->callback_arg = arg;
}

void ParserSetDtdCallback(Parser p, CallbackProc cb)
{
    p->dtd_callback = cb;
}

void ParserSetWarningCallback(Parser p, CallbackProc cb)
{
    p->warning_callback = cb;
}

void ParserSetEntityOpener(Parser p, EntityOpenerProc opener)
{
    p->entity_opener = opener;
}

#ifndef FOR_LT

XBit ReadXTree(Parser p)
{
    XBit bit, tree, child;
    XBit *children;

    bit = ReadXBit(p);

    switch(bit->type)
    {
    case XBIT_error:
	return bit;

    case XBIT_start:
	if(!(tree = Malloc(sizeof(*tree))))
	{
	    error(p, "System error");
	    return &p->xbit;
	}
	*tree = *bit;
	while(1)
	{
	    child = ReadXTree(p);
	    switch(child->type)
	    {
	    case XBIT_error:
		FreeXTree(tree);
		return child;
		
	    case XBIT_eof:
		FreeXTree(tree);
		{
		    error(p, "EOF in element");
		    return &p->xbit;
		}

	    case XBIT_end:
		if(child->element_definition != tree->element_definition)
		{
		    const Char *name1 = tree->element_definition->name,
			       *name2 = child->element_definition->name;
		    FreeXTree(tree);
		    FreeXTree(child);
		    error(p, "Mismatched end tag: expected </%S>, got </%S>",
			  name1, name2);
		    return &p->xbit;
		}
		FreeXTree(child);
		return tree;

	    default:
		children = Realloc(tree->children, 
				   (tree->nchildren + 1) * sizeof(XBit));
		if(!children)
		{
		    FreeXTree(tree);
		    FreeXTree(child);
		    error(p, "System error");
		    return &p->xbit;
		}
		child->parent = tree;
		children[tree->nchildren] = child;
		tree->nchildren++;
		tree->children = children;
		break;
	    }
	}
	
    default:
	if(!(tree = Malloc(sizeof(*tree))))
	{
	    error(p, "System error");
	    return &p->xbit;
	}
	*tree = *bit;
	return tree;
    }
}

void FreeXTree(XBit tree)
{
    int i;

    for(i=0; i<tree->nchildren; i++)
	FreeXTree(tree->children[i]);

    Free(tree->children);

    FreeXBit(tree);

    if(tree->type == XBIT_error)
	/* error "trees" are always in the Parser structure, not malloced */
	return;

    Free(tree);
}

#endif /* (not) FOR_LT */

XBit ReadXBit(Parser p)
{
    if(p->peeked)
	p->peeked = 0;
    else
	parse(p);

    return &p->xbit;
}

XBit PeekXBit(Parser p)
{
    if(p->peeked)
	error(p, "Attempt to peek twice");
    else
    {
	parse(p);
	p->peeked = 1;
    }

    return &p->xbit;
}

int ParserPush(Parser p, InputSource source)
{
    if(!p->source && !p->document_entity)
	p->document_entity = source->entity;

    source->parent = p->source;
    p->source = source;

    if(source->entity->type == ET_internal)
	return 0;

    /* Look at first few bytes of external entities to guess encoding,
       then look for an XMLDecl or TextDecl.  */

    if(source->entity->encoding == CE_unknown) /* we might already know */
	determine_character_encoding(source);

#if CHAR_SIZE == 8
    if(!EncodingIsAsciiSuperset(source->entity->encoding))
	return error(p, "Unsupported character encoding %s",
		     CharacterEncodingName[source->entity->encoding]);
#else
    if(source->entity->encoding == CE_unknown)
	return error(p, "Unknown character encoding");
#endif

    get(source); unget(source);	/* To get the first line read */

    source->entity->ml_decl = ML_unspecified;
    if(looking_at(p, "<?NSL "))
	return process_nsl_decl(p);
    if(looking_at(p, "<?xml "))
    {
	require(process_xml_decl(p));
	if(source->entity == p->document_entity &&
	   !source->entity->version_decl)
	    return error(p, "XML declaration in document entity lacked "
			    "version number");
	if(source->entity != p->document_entity &&
	   source->entity->standalone_decl != SDD_unspecified)
	    return error(p, "Standalone attribute not allowed except in "
			    "document entity");
	return 0;
    }
    else if(!ParserGetFlag(p, XMLStrictWFErrors) && looking_at(p, "<?XML "))
    {
	warn(p, "Found <?XML instead of <?xml; switching to case-"
	     "insensitive mode");
	ParserSetFlag(p, CaseInsensitive, 1);
	return process_xml_decl(p);
    }
    else
	return 0;
}

void ParserPop(Parser p)
{
    InputSource source;

    source = p->source;
    Fclose(source->file16);
    p->source = source->parent;

    if(source->entity->type == ET_external)
	Free(source->line);
    Free(source);
}

/* Returns true if the source is at EOE. If so, the EOE will have been read. */

static int at_eoe(InputSource s)
{
    if(!at_eol(s))
	return 0;
    if(s->seen_eoe || get_with_fill(s) == XEOE)
	return 1;
    unget(s);
    return 0;
}

/* Pops any sources that are at EOE.  Leaves source buffer with at least
   one character in it (except at EOF, where it leaves the EOE unread). */

static void pop_while_at_eoe(Parser p)
{
    while(1)
    {
	InputSource s = p->source;

	if(!at_eoe(s))
	    return;
	if(!s->parent)
	{
	    unget(s);
	    return;
	}
	ParserPop(p);
    }
}

void ParserSetFlag(Parser p, ParserFlag flag, int value)
{
    if(value)
	p->flags |= (1 << flag);
    else
	p->flags &= ~(1 << flag);

    if(flag == XMLPredefinedEntities)
    {
	if(value)
	    p->dtd->predefined_entities = xml_predefined_entities;
	else
	    p->dtd->predefined_entities = 0;
    }
}

void ParserPerror(Parser p, XBit bit)
{
    int linenum, charnum;
    InputSource s;

    Fprintf(Stderr, "%s: %s\n", 
	    bit->type == XBIT_error ? "Error" : "Warning", 
	    bit->error_message);


    for(s=p->source; s; s=s->parent)
    {
	if(s->entity->name)
	    Fprintf(Stderr, " in entity \"%S\"", s->entity->name);
	else
	    Fprintf(Stderr, " in unnamed entity");

	switch(SourceLineAndChar(s, &linenum, &charnum))
	{
	case 1:
	    Fprintf(Stderr, " at line %d char %d of", linenum+1, charnum+1);
	    break;
	case 0:
	    Fprintf(Stderr, " defined at line %d char %d of",
		    linenum+1, charnum+1);
	    break;
	case -1:
	    Fprintf(Stderr, " defined in");
	    break;
	}

	Fprintf(Stderr, " %s\n", EntityDescription(s->entity));
    }
}


static int parse(Parser p)
{
    int c;
    InputSource s;

    if(p->state == PS_end || p->state == PS_error)
    {
	/* After an error or EOF, just keep returning EOF */
	p->xbit.type = XBIT_eof;
	return 0;
    }

    clear_xbit(&p->xbit);
    
    if(p->state <= PS_prolog2 || p->state == PS_epilog)
	skip_whitespace(p->source);

restart:
    pop_while_at_eoe(p);
    s = p->source;
    SourcePosition(s, &p->xbit.entity, &p->xbit.byte_offset);

    switch(c = get(s))
    {
    case XEOE:
	if(p->state != PS_epilog)
	    return error(p, "Document ends too soon");
	p->state = PS_end;
	p->xbit.type = XBIT_eof;
	return 0;
    case '<':
	return parse_markup(p);
    case '&':
	if(ParserGetFlag(p, IgnoreEntities))
	    goto pcdata;
	if(p->state <= PS_prolog2)
	    return error(p, "Entity reference not allowed in prolog");
	if(looking_at(p, "#"))
	{
	    /* a character reference - go back and parse as pcdata */
	    unget(s);
	    goto pcdata;
	}
	if(ParserGetFlag(p, ExpandGeneralEntities))
	{
	    /* an entity reference - push it and start again */
	    require(parse_reference(p, 0, 1, 1));
	    goto restart;
	}
	/* not expanding general entities, so treat as pcdata */
	goto pcdata;
    default:
    pcdata:
	unget(s);
	return parse_pcdata(p);
    }
}

/* Called after reading '<' */

static int parse_markup(Parser p)
{
    InputSource s = p->source;
    int c = get(s);

    switch(c)
    {
    case '!':
	if(looking_at(p, "--"))
	{
	    if(ParserGetFlag(p, ReturnComments))
		return parse_comment(p, 0);
	    else
	    {
		require(parse_comment(p, 1));
		return parse(p);
	    }
	}
	else if(looking_at(p, "DOCTYPE "))
	    return parse_dtd(p);
	else if(looking_at(p, "[CDATA["))
	    return parse_cdata(p);
	else
	    return error(p, "Syntax error after <!");
	
    case '/':
	return parse_endtag(p);

    case '?':
	return parse_pi(p);

    default:
	unget(s);
	if(!ParserGetFlag(p, XMLLessThan) && 
	   (c == XEOE || !is_xml_namestart(c)))
	{
	    /* In nSGML, recognise < as stago only if followed by namestart */

	    unget(s);	/* put back the < */
	    return parse_pcdata(p);
	}
	return parse_starttag(p);
    }
}

static int parse_endtag(Parser p)
{
    ElementDefinition def;
    Entity ent;

    p->xbit.type = XBIT_end;
    require(parse_name(p, "after </"));
    maybe_uppercase_name(p);

    if(ParserGetFlag(p, CheckEndTagsMatch))
    {
	if(p->element_depth <= 0)
	    return error(p, "End tag </%.*S> outside of any element",
			 p->namelen, p->name);

	ent = p->element_stack[--p->element_depth].entity;
	def = p->element_stack[p->element_depth].definition;

	if(p->namelen == def->namelen &&
	   memcmp(p->name, def->name, p->namelen * sizeof(Char)) == 0)
	    p->xbit.element_definition = def;
	else
	    return error(p, "Mismatched end tag: expected </%S>, got </%.*S>",
			 def->name, p->namelen, p->name);

	if(ent != p->source->entity)
	    return error(p, "Element ends in different entity from that "
			    "in which it starts");

	if(p->element_depth == 0)
	    p->state = PS_epilog;
    }
    else
    {
	p->xbit.element_definition = FindElementN(p->dtd, p->name, p->namelen);
	if(!p->xbit.element_definition)
	    return error(p, "End tag for unknown element %.*S", 
			 p->namelen, p->name);
    }

    skip_whitespace(p->source);
    return expect(p, '>', "after name in end tag");
}

static int parse_starttag(Parser p)
{
    int c;

    if(p->state == PS_epilog && !ParserGetFlag(p, AllowMultipleElements))
	return error(p, "Document contains multiple elements");

    p->state = PS_body;

    require(parse_name(p, "after <"));
    maybe_uppercase_name(p);

    p->xbit.element_definition = FindElementN(p->dtd, p->name, p->namelen);
    if(!p->xbit.element_definition || p->xbit.element_definition->tentative)
    {
	if(p->have_dtd && ParserGetFlag(p, ErrorOnUndefinedElements))
	    return error(p, "Start tag for undeclared element %.*S",
			 p->namelen, p->name);
	if(p->have_dtd && ParserGetFlag(p, WarnOnUndefinedElements))
	    warn(p, "Start tag for undeclared element %.*S; "
		    "declaring it to have content ANY",
		 p->namelen, p->name);
	if(p->xbit.element_definition)
	    RedefineElement(p->xbit.element_definition, CT_any, 0);
	else
	{
	    if(!(p->xbit.element_definition = 
		 DefineElementN(p->dtd, p->name, p->namelen, CT_any, 0)))
		return error(p, "System error");
	}
    }

    while(1)
    {
	InputSource s = p->source;

	/* We could just do skip_whitespace here, but we will get a
	   better error message if we look a bit closer. */

	c = get(s);
	if(c !=XEOE && is_xml_whitespace(c))
	{
	    skip_whitespace(s);
	    c = get(s);
	}
	else if(c != '>' &&
		!(ParserGetFlag(p, XMLEmptyTagEnd) && c == '/'))
	{
	    unget(s);		/* For error position */
	    return error(p, "Expected whitespace or tag end in start tag");
	}

	if(c == '>')
	{
	    p->xbit.type = XBIT_start;
	    break;
	}

	if((ParserGetFlag(p, XMLEmptyTagEnd)) && c == '/')
	{
	    require(expect(p, '>', "after / in start tag"));
	    p->xbit.type = XBIT_empty;
	    break;
	}

	unget(s);

	require(parse_attribute(p));
    }

    if(ParserGetFlag(p, CheckEndTagsMatch))
    {
	if(p->xbit.type == XBIT_start)
	{
	    if(p->element_depth == p->element_stack_alloc)
	    {
		p->element_stack_alloc = 
		    p->element_stack_alloc == 0 ? 20 :
		                                  p->element_stack_alloc * 2;
		if(!(p->element_stack = 
		     Realloc(p->element_stack,
			(p->element_stack_alloc * sizeof(*p->element_stack)))))
		    return error(p, "System error");
	    }
	    p->element_stack[p->element_depth].definition =
		p->xbit.element_definition;
	    p->element_stack[p->element_depth++].entity = p->source->entity;
	}
	else
	    if(p->element_depth == 0)
		p->state = PS_epilog;
    }

    if(ParserGetFlag(p, ReturnDefaultedAttributes))
    {
	AttributeDefinition d;
	Attribute a;

	for(d=NextAttributeDefinition(p->xbit.element_definition, 0);
	    d;
	    d=NextAttributeDefinition(p->xbit.element_definition, d))
	{
	    if(!d->default_value)
		continue;
	    for(a=p->xbit.attributes; a; a=a->next)
		if(a->definition == d)
		    break;
	    if(!a)
	    {
		if(!(a = Malloc(sizeof(*a))))
		    return error(p, "System error");
		a->definition = d;
		if(!(a->value = Strdup(d->default_value)))
		    return error(p, "System error");
		a->quoted = 1;
		a->next = p->xbit.attributes;
		p->xbit.attributes = a;
	    }
	}	
    }

    return 0;
}

static int parse_attribute(Parser p)
{
    InputSource s = p->source;
    AttributeDefinition def;
    struct attribute *a;
    int c;

    require(parse_name(p, "for attribute"));
    maybe_uppercase_name(p);

    def = FindAttributeN(p->xbit.element_definition, p->name, p->namelen);
    if(!def)
    {
	if(p->have_dtd && ParserGetFlag(p, ErrorOnUndefinedAttributes))
	    return error(p, "Undeclared attribute %.*S for element %S",
			p->namelen, p->name, p->xbit.element_definition->name);
	if(p->have_dtd && ParserGetFlag(p, WarnOnUndefinedAttributes))
	    warn(p, "Undeclared attribute %.*S for element %S; "
		    "declaring it as CDATA #IMPLIED",
		 p->namelen, p->name, p->xbit.element_definition->name);
	if(!(def = DefineAttributeN(p->xbit.element_definition, 
				    p->name, p->namelen,
				    AT_cdata, 0, DT_implied, 0)))
	    return error(p, "System error");
    }

    for(a = p->xbit.attributes; a; a = a->next)
	if(a->definition == def)
	    return error(p, "Repeated attribute %.*S", p->namelen, p->name);

    if(!(a = Malloc(sizeof(*a))))
	return error(p, "System error");

    a->value = 0;		/* in case of error */
    a->next = p->xbit.attributes;
    p->xbit.attributes = a;
    a->definition = def;

    skip_whitespace(s);
    require(expect(p, '=', "after attribute name"));

    skip_whitespace(s);
    c = get(s);
    unget(s);
    switch(c)
    {
    case '"':
    case '\'':
	a->quoted = 1;
	require(parse_string(p, "in attribute value",
			     a->definition->type == AT_cdata ? LT_cdata_attr :
			                                       LT_tok_attr));
	a->value = p->pbuf;
	Consume(p->pbuf);
	break;
    default:
	if(ParserGetFlag(p, ErrorOnUnquotedAttributeValues))
	    return error(p, "Value of attribute is unquoted");
	a->quoted = 0;
	require(parse_nmtoken(p, "in unquoted attribute value"));
	CopyName(a->value);
	break;
    }

    return 0;
}

static int transcribe(Parser p, int back, int count)
{
    ExpandBuf(p->pbuf, p->pbufnext + count);
    memcpy(p->pbuf + p->pbufnext,
	   p->source->line + p->source->next - back,
	   count * sizeof(Char));
    p->pbufnext += count;
    return 0;
}

/* Called after pushing back the first character of the pcdata */

static int parse_pcdata(Parser p)
{
    int count = 0;
    InputSource s;
    Char *buf;
    int next, buflen;

    if(p->state <= PS_prolog2)
	return error(p, "Character data not allowed in prolog");
    if(p->state == PS_epilog)
	return error(p, "Character data not allowed after body");

    s = p->source;
    buf = s->line;
    next = s->next;
    buflen = s->line_length;

    p->pbufnext = 0;
    
    while(1)
    {
	if(next == buflen)
	{
	    s->next = next;
	    if(count > 0)
	    {
		require(transcribe(p, count, count));
	    }
	    count = 0;
	    if(at_eoe(s))
	    {
		if(!ParserGetFlag(p, MergePCData))
		    goto done;
		else
		    pop_while_at_eoe(p);
	    }
	    s = p->source;
	    buf = s->line;
	    next = s->next;
	    buflen = s->line_length;
	    if(next == buflen)
		goto done;	/* must be EOF */
	}

	switch(buf[next++])
	{
	case '<':
	    if(!ParserGetFlag(p, XMLLessThan))
	    {
		/* In nSGML, don't recognise < as markup unless it looks ok */
		if(next == buflen)
		    goto deflt;
		if(buf[next] != '!' && buf[next] != '/' && buf[next] != '?' &&
		   !is_xml_namestart(buf[next]))
		    goto deflt;
	    }
	    s->next = next;
	    if(count > 0)
	    {
		require(transcribe(p, count+1, count));
	    }
	    count = 0;
	    if(!ParserGetFlag(p, ReturnComments) && 
	       buflen >= next + 3 &&
	       buf[next] == '!' && buf[next+1] == '-' && buf[next+2] == '-')
	    {
		s->next = next + 3;
		require(parse_comment(p, 1));
		buflen = s->line_length;
		next = s->next;
	    }
	    else
	    {
		s->next = next-1;
		goto done;
	    }
	    break;
	case '&':
	    if(ParserGetFlag(p, IgnoreEntities))
		goto deflt;
	    if(!ParserGetFlag(p, MergePCData) &&
	       (p->pbufnext > 0  || count > 0))
	    {
		/* We're returning references as separate bits, and we've
		 come to one, and we've already got some data to return,
		 so return what we've got and get the reference next time. */

		s->next = next-1;
		if(count > 0)
	        {
		    require(transcribe(p, count, count));
	        }
		goto done;
	    }
	    if(buflen >= next+1 && buf[next] == '#')
	    {
		/* It's a character reference */

		s->next = next+1;
		if(count > 0)
	        {
		    require(transcribe(p, count+2, count));
	        }
		count = 0;
		require(parse_character_reference(p,
				   ParserGetFlag(p, ExpandCharacterEntities)));
		next = s->next;

		if(!ParserGetFlag(p, MergePCData))
		    goto done;
	    }
	    else
	    {
		/* It's a general entity reference */

		s->next = next;
		if(count > 0)
	        {
		    require(transcribe(p, count+1, count));
	        }
		count = 0;
		require(parse_reference(p, 0, 
				       ParserGetFlag(p, ExpandGeneralEntities),
					1));
		s = p->source;
		buf = s->line;
		buflen = s->line_length;
		next = s->next;

		if(!ParserGetFlag(p, MergePCData))
		    goto done;
	    }
	    break;
	case ']':
	    if(ParserGetFlag(p, XMLMiscWFErrors) &&
	       buflen >= next + 2 &&
	       buf[next] == ']' && buf[next+1] == '>')
		return error(p, "Illegal character sequence ']]>' in pcdata");
	    /* fall through */
	default:
	deflt:
	    count++;
	    break;
	}
    }
    
  done:
    p->pbuf[p->pbufnext++] = 0;
    p->xbit.type = XBIT_pcdata;
    p->xbit.pcdata_chars = p->pbuf;
    Consume(p->pbuf);

    return 0;
}

/* Called after reading '<!--'.  Won't go over an entity end. */

static int parse_comment(Parser p, int skip)
{
    InputSource s = p->source;
    int c, c1=0, c2=0;
    int count = 0;
    
    if(!skip)
	p->pbufnext = 0;

    while((c = get(s)) != XEOE)
    {
	count++;
	if(c1 == '-' && c2 == '-')
	{
	    if(c == '>')
		break;
	    unget(s);		/* For error position */
	    return error(p, "-- in comment");
	}
	    
	if(at_eol(s))
	{
	    if(!skip)
	    {
		require(transcribe(p, count, count));
	    }
	    count = 0;
	}
	c2 = c1; c1 = c;
    }

    if(c == XEOE)
	return error(p, "EOE in comment");

    if(skip)
	return 0;

    require(transcribe(p, count, count-3));
    p->pbuf[p->pbufnext++] = 0;
    p->xbit.type = XBIT_comment;
    p->xbit.comment_chars = p->pbuf;
    Consume(p->pbuf);

    return 0;
}

static int parse_pi(Parser p)
{
    InputSource s = p->source;
    int c, c1=0;
    int count = 0;
    Char xml[] = {'x', 'm', 'l', 0};

    require(parse_name(p, "after <?"));
    CopyName(p->xbit.pi_name);

    p->pbufnext = 0;

    if(Strcasecmp(p->xbit.pi_name, xml) == 0)
    {
	if(ParserGetFlag(p, XMLStrictWFErrors))
	    return error(p, "Misplaced or wrong-case xml declaration");
	else
	    warn(p, "Misplaced or wrong-case xml declaration; treating as PI");
    }

    /* Empty PI? */

    if(looking_at(p, ParserGetFlag(p, XMLPiEnd) ? "?>" : ">"))
    {
	ExpandBuf(p->pbuf, 0);
	goto done;
    }

    /* If non-empty, must be white space after name */

    c = get(s);
    if(c == XEOE || !is_xml_whitespace(c))
	return error(p, "Expected whitespace after PI name");
    skip_whitespace(s);

    while((c = get(s)) != XEOE)
    {
	count++;
	if(c == '>' &&
	   (!ParserGetFlag(p, XMLPiEnd) || c1 == '?'))
	    break;
	if(at_eol(s))
	{
	    require(transcribe(p, count, count));
	    count = 0;
	}
	c1 = c;
    }

    if(c == XEOE)
	return error(p, "EOE in PI");

    require(transcribe(p, count, count-(ParserGetFlag(p, XMLPiEnd) ? 2 : 1)));
done:
    p->pbuf[p->pbufnext++] = 0;
    p->xbit.type = XBIT_pi;
    p->xbit.pi_chars = p->pbuf;
    Consume(p->pbuf);

    return 0;
}

static int parse_string(Parser p, const char8 *where, enum literal_type type)
{
    int c, quote;
    int count = 0;
    InputSource start_source, s;

    s = start_source = p->source;
    
    quote = get(s);
    if(quote != '\'' && quote != '"')
    {
	unget(s);		/* For error position */
	return error(p, "Expected quoted string %s, but got %s",
		     where, escape(quote));
    }

    p->pbufnext = 0;

    while(1)
    {
	switch(c = get(s))
	{
	case '\r':
	case '\n':
	case '\t':
	    if(type == LT_plain || type == LT_entity ||
	       !ParserGetFlag(p, NormaliseAttributeValues))
	    {
		count++;
		break;
	    }
	    if(count > 0)
	    {
		require(transcribe(p, count+1, count));
	    }
	    count = 0;
	    ExpandBuf(p->pbuf, p->pbufnext+1);
	    p->pbuf[p->pbufnext++] = ' ';
	    break;

	case '<':
	    if((type == LT_tok_attr || type == LT_cdata_attr) &&
	       ParserGetFlag(p, XMLMiscWFErrors))
		return error(p, "Illegal character '<' %s", where);
	    count++;
	    break;

	case XEOE:
	    if(s == start_source)
	    {
		return error(p, "Quoted string goes past entity end");
	    }
	    if(count > 0)
	    {
		require(transcribe(p, count, count));
	    }
	    count = 0;
	    ParserPop(p);
	    s = p->source;
	    break;

	case '%':
	    if(type != LT_entity)
	    {
		count++;
		break;
	    }
	    if(count > 0)
	    {
		require(transcribe(p, count+1, count));
	    }
	    count = 0;
	    if(p->external_pe_depth == 0)
	    {
		unget(s);	/* For error position */
		return error(p, "PE ref not allowed here in internal subset");
	    }
	    require(parse_reference(p, 1, 1, 1));
	    s = p->source;
	    break;

	case '&':
	    if(ParserGetFlag(p, IgnoreEntities))
		goto deflt;
	    if(type == LT_plain)
	    {
		count++;
		break;
	    }

	    if(count > 0)
	    {
		require(transcribe(p, count+1, count));
	    }
	    count = 0;
	    if(looking_at(p, "#"))
		require(parse_character_reference(p, 
				ParserGetFlag(p, ExpandCharacterEntities)));
	    else
	    {
		require(parse_reference(p, 0,
				       type != LT_entity &&
				       ParserGetFlag(p, ExpandGeneralEntities),
				       !ParserGetFlag(p, XMLMiscWFErrors)));
		s = p->source;
	    }
	    break;

	default:
	deflt:
	    if(c == quote && p->source == start_source)
		goto done;
	    count++;
	}

	if(at_eol(s) && count > 0)
	{
	    require(transcribe(p, count, count));
	    count = 0;
	}
    }

done:
    if(count > 0)
	require(transcribe(p, count+1, count));
    else
	ExpandBuf(p->pbuf, p->pbufnext+1);
    p->pbuf[p->pbufnext++] = 0;

    if(ParserGetFlag(p, NormaliseAttributeValues) && type == LT_tok_attr)
    {
	Char *old, *new;

	new = old = p->pbuf;

	/* Maybe skip leading whitespace */

	while(*old == ' ')
	    old++;

	/* Translate whitespace to spaces, maybe compressing */

	for( ; *old; old++)
	{
	    if(*old == ' ')
	    {
		/* NB can't be at start because we skipped whitespace */
		if(type == LT_tok_attr && new[-1] == ' ')
		    ;
		else
		    *new++ = ' ';
	    }
	    else
		*new++ = *old;
	}

	/* Maybe trim trailing space (only one possible) */

	if(new > p->pbuf && new[-1] == ' ')
	    new--;

	*new = 0;
    }

    return 0;
}

static int parse_dtd(Parser p)
{
    InputSource s = p->source;
    Entity parent = s->entity;
    Entity internal_part = 0, external_part = 0;
    Char *name;
    char8 *publicid = 0, *systemid = 0;
    struct xbit xbit;

    xbit = p->xbit;		/* copy start position */
    xbit.type = XBIT_dtd;

    require(parse_name(p, "for name in dtd"));
    CopyName(name);
    maybe_uppercase(p, name);

    skip_whitespace(s);

    require(parse_external_id(p, 0, &publicid, &systemid, 
			      ParserGetFlag(p, XMLExternalIDs),
			      ParserGetFlag(p, XMLExternalIDs)));

    if(systemid || publicid)
    {
	external_part = NewExternalEntity(0, publicid, systemid, 0, parent);
	if(!external_part)
	{
	    Free(name);
	    return error(p, "System error");
	}
	skip_whitespace(s);
    }

    if(looking_at(p, "["))
    {
	int line = s->line_number, cpos = s->next;

	require(read_markupdecls(p));
	skip_whitespace(s);
	internal_part = NewInternalEntity(0, p->pbuf, parent, line, cpos, 1);
	Consume(p->pbuf);
	if(!internal_part)
	{
	    Free(name);
	    FreeEntity(external_part);
	    return error(p, "System error");
	}
    }

    require(expect(p, '>', "at end of dtd"));

    if(p->state == PS_prolog1)
	p->state = PS_prolog2;
    else
    {
	Free(name);
	FreeEntity(external_part);
	FreeEntity(internal_part);

	if(ParserGetFlag(p, XMLStrictWFErrors))
	    return error(p, "Misplaced or repeated DOCTYPE declaration");

	warn(p, "Misplaced or repeated DOCTYPE declaration");
	/* Ignore it and return the next bit */
	return parse(p);
    }

    if(p->dtd->name)
    {
	Free(name);
	FreeEntity(external_part);
	FreeEntity(internal_part);

	/* This happens if we manually set the dtd */
	return parse(p);
    }

    p->dtd->name = name;
    p->dtd->internal_part = internal_part;
    p->dtd->external_part = external_part;

    if(ParserGetFlag(p, TrustSDD))
    {
	if(internal_part)
	{
	    ParseDtd(p, internal_part);
	    if(p->xbit.type == XBIT_error)
		return -1;
	}
	if(external_part && p->standalone != SDD_yes)
	{
	    ParseDtd(p, external_part);
	    if(p->xbit.type == XBIT_error)
		return -1;
	}
    }

    p->xbit = xbit;
    return 0;
}

static int read_markupdecls(Parser p)
{
    InputSource s = p->source;
    int depth=1;
    int c, d, hyphens=0;
    int count = 0;

    p->pbufnext = 0;

    while(1)
    {
	c = get(s);
	if(c == XEOE)
	    return error(p, "EOE in DTD");
	if(c == '-')
	    hyphens++;
	else
	    hyphens = 0;

	count++;

	switch(c)
	{
	case ']':
	    if(--depth == 0)
	    {
		count--;	/* We don't want the final ']' */
		require(transcribe(p, count+1, count));
		p->pbuf[p->pbufnext++] = 0;
		return 0;
	    }
	    break;

	case '[':
	    depth++;
	    break;

	case '"':
	case '\'':
	    while((d = get(s)) != XEOE)
	    {
		count++;
		if(at_eol(s))
		{
		    require(transcribe(p, count, count));
		    count = 0;
		}
		if(d == c)
		    break;
	    }
	    if(d == XEOE)
		return error(p, "EOE in DTD");
	    break;

	case '-':
	    if(hyphens < 2)
		break;
	    hyphens = 0;
	    while((d = get(s)) != XEOE)
	    {
		count++;
		if(at_eol(s))
		{
		    require(transcribe(p, count, count));
		    count = 0;
		}
		if(d == '-')
		    hyphens++;
		else
		    hyphens = 0;
		if(hyphens == 2)
		    break;
	    }
	    if(d == XEOE)
		return error(p, "EOE in DTD");
	    hyphens = 0;
	    break;

	default:
	    break;
	}

	if(at_eol(s) && count > 0)
	{
	    require(transcribe(p, count, count));
	    count = 0;
	}
    }
}

static int process_nsl_decl(Parser p)
{
    InputSource s = p->source;
    int c, count = 0;

    s->entity->ml_decl = ML_nsl;

    /* The default character encoding for nSGML files is ascii-ash */
    if(s->entity->encoding == CE_UTF_8)
	s->entity->encoding = CE_unspecified_ascii_superset;

    /* Syntax is <?NSL DDB unquoted-filename 0> */

    if(!looking_at(p, "DDB "))
	return error(p, "Expected \"DDB\" in NSL declaration");

    while(c = get(s), !is_xml_whitespace(c))
	switch(c)
	{
	case XEOE:
	    return error(p, "EOE in NSL declaration");

	case '>':
	    return error(p, "Syntax error in NSL declaration");
	    
	default:
	    count++;
	}

    p->pbufnext = 0;
    require(transcribe(p, count+1, count));
    p->pbuf[p->pbufnext++] = 0;

    skip_whitespace(s);
    if(!looking_at(p, "0>"))
	return error(p, "Expected \"0>\" at end of NSL declaration");

    if(!(s->entity->ddb_filename = strdup8(Chartochar8(p->pbuf))))
	return error(p, "System error");

    return 0;
}

static int process_xml_decl(Parser p)
{
    InputSource s = p->source;
    enum {None, V, E, S} which, last = None;
    Char *Value, *cp;
    char8 *value;
    CharacterEncoding enc = CE_unknown;
    Char c;

    s->entity->ml_decl = ML_xml;

    /* XXX Should save the string buffer because it may already be in use */

    while(!looking_at(p, "?>"))
    {
	if(looking_at(p, "version"))
	    which = V;
	else if(looking_at(p, "encoding"))
	    which = E;
	else if(looking_at(p, "standalone"))
	    which = S;
	else
	    return error(p, "Expected \"version\", \"encoding\" or "
			 "\"standalone\" in XML declaration");

	if(which <= last)
	{
	    if(ParserGetFlag(p, XMLStrictWFErrors))
		return error(p, "Repeated or misordered attributes "
			        "in XML declaration");
	    warn(p, "Repeated or misordered attributes in XML declaration");
	}
	last = which;
	
	skip_whitespace(s);
	require(expect(p, '=', "after attribute name in XML declaration"));
	skip_whitespace(s);

	require(parse_string(p, "for attribute value in XML declaration",
			     LT_plain));

	maybe_uppercase(p, p->pbuf);
	Value = p->pbuf;

	if(which == E)
	{
	    if(!is_ascii_alpha(Value[0]))
		return error(p, "Encoding name does not begin with letter");
	    for(cp=Value+1; *cp; cp++)
		if(!is_ascii_alpha(*cp) && !is_ascii_digit(*cp) &&
		   *cp != '.' && *cp != '_' && *cp != '-')
		    return error(p, "Illegal character %s in encoding name",
				 escape(*cp));

	    value = Chartochar8(Value);

	    enc = FindEncoding(value);
	    if(enc == CE_unknown)
		return error(p, "Unknown declared encoding %s", value);

	    if(EncodingsCompatible(p->source->entity->encoding, enc, &enc))
	    {
#if CHAR_SIZE == 8
		/* We ignore the declared encoding in 8-bit mode,
		   and treat it as a random ascii superset. */
#else
		p->source->entity->encoding = enc;
#endif
	    }
	    else
		return error(p, "Declared encoding %s is incompatible with %s "
			        "which was used to read it",
		     CharacterEncodingName[enc],
		     CharacterEncodingName[p->source->entity->encoding]);

	    s->entity->encoding_decl = enc;
	}

	if(which == S)
	{
	    value = Chartochar8(Value);

	    if(str_maybecase_cmp8(p, value, "no") == 0)
		p->standalone = SDD_no;
	    else if(str_maybecase_cmp8(p, value, "yes") == 0)
		p->standalone = SDD_yes;
	    else
		return error(p, "Expected \"yes\" or \"no\" "
			        "for standalone in XML declaration");

	    s->entity->standalone_decl = p->standalone;
	}

	if(which == V)
	{
	    for(cp=Value; *cp; cp++)
		if(!is_ascii_alpha(*cp) && !is_ascii_digit(*cp) &&
		   *cp != '.' && *cp != '_' && *cp != '-' && *cp != ':')
		    return error(p, "Illegal character %s in version number",
				 escape(*cp));

	    if(!s->entity->version_decl)
		if(!(s->entity->version_decl = strdup8(Chartochar8(Value))))
		    return error(p, "System error");
	}

	c = get(s);
	if(c == '?')
	    unget(s);
	else if(!is_xml_whitespace(c))
	    return error(p, "Expected whitespace or \"?>\" after attribute "
			    "in XML declaration");
	skip_whitespace(s);
    }
    return 0;
}

static int parse_cdata(Parser p)
{
    InputSource s = p->source;
    int c, c1=0, c2=0;
    int count = 0;

    if(p->state <= PS_prolog2)
	return error(p, "Cdata section not allowed in prolog");
    if(p->state == PS_epilog)
	return error(p, "Cdata section not allowed after body");

    p->pbufnext = 0;

    while((c = get(s)) != XEOE)
    {
	count++;
	if(c == '>' && c1 == ']' && c2 == ']')
	    break;
	if(at_eol(s))
	{
	    require(transcribe(p, count, count));
	    count = 0;
	}
	c2 = c1; c1 = c;
    }

    if(c == XEOE)
	return error(p, "EOE in CData section");

    require(transcribe(p, count, count-3));
    p->pbuf[p->pbufnext++] = 0;
    p->xbit.type = XBIT_cdsect;
    p->xbit.cdsect_chars = p->pbuf;
    Consume(p->pbuf);

    return 0;
}

XBit ParseDtd(Parser p, Entity e)
{
    InputSource source, save;

    if(e->type == ET_external && p->entity_opener)
	source = p->entity_opener(e, p->callback_arg);
    else
	source = EntityOpen(e);
    if(!source)
    {
	error(p, "Couldn't open dtd entity %s", EntityDescription(e));
	return &p->xbit;
    }

    save = p->source;
    p->source = 0;
    if(ParserPush(p, source) == -1)
	return &p->xbit;

    p->have_dtd = 1;

    p->external_pe_depth = (source->entity->type == ET_external);

    while(parse_markupdecl(p) == 0)
	;

    p->external_pe_depth = 0;

    /* don't restore after error, so user can call ParserPerror */
    if(p->xbit.type != XBIT_error)
    {
	ParserPop(p);		/* to free the input source */
	p->source = save;
    }

    return &p->xbit;
}

/* 
 * Returns 0 normally, -1 if error, 1 at EOF.
 */
static int parse_markupdecl(Parser p)
{
    InputSource s;
    int c;
    int cur_line, cur_char;
    Entity cur_ent;

    if(p->state == PS_error)
	return error(p, "Attempt to continue reading DTD after error");

    clear_xbit(&p->xbit);

    require(skip_dtd_whitespace(p, 1));	/* allow PE even in internal subset */
    s = p->source;
    SourcePosition(s, &p->xbit.entity, &p->xbit.byte_offset);

    cur_ent = s->entity;
    cur_line = s->line_number;
    cur_char = s->next;

    c = get(s);
    switch(c)
    {
    case XEOE:
	p->xbit.type = XBIT_none;
	return 1;
    case '<':
	if(looking_at(p, "!ELEMENT"))
	{
	    require(expect_dtd_whitespace(p, "after ELEMENT"));
	    return parse_element_decl(p);
	}
	else if(looking_at(p, "!ATTLIST"))
	{
	    require(expect_dtd_whitespace(p, "after ATTLIST"));
	    return parse_attlist_decl(p);
	}
	else if(looking_at(p, "!ENTITY"))
	{
	    require(expect_dtd_whitespace(p, "after ENTITY"));
	    return parse_entity_decl(p, cur_ent, cur_line, cur_char);
	}
	else if(looking_at(p, "!NOTATION"))
	{
	    require(expect_dtd_whitespace(p, "after NOTATION"));
	    return parse_notation_decl(p);
	}
	else if(looking_at(p, "!["))
	    return parse_conditional(p);
	else if(looking_at(p, "?"))
	{
	    require(parse_pi(p));
	    if(p->dtd_callback)
		p->dtd_callback(&p->xbit, p->callback_arg);
	    else
		FreeXBit(&p->xbit);
	    return 0;
	}
	else if(looking_at(p, "!--"))
	{
	    if(ParserGetFlag(p, ReturnComments))
	    {
		require(parse_comment(p, 0));
		if(p->dtd_callback)
		    p->dtd_callback(&p->xbit, p->callback_arg);
		else
		    FreeXBit(&p->xbit);
		return 0;
	    }
	    else
		return parse_comment(p, 1);
	}
	else
	    return error(p, "Syntax error after < in dtd");
    default:
	unget(s);		/* For error position */
	return error(p, "Expected \"<\" in dtd, but got %s", escape(c));
    }
}

static int parse_reference(Parser p, int pe, int expand, int allow_external)
{
    Entity e;
    InputSource s;

    require(parse_name(p, pe ? "for parameter entity" : "for entity"));
    require(expect(p, ';', "after entity name"));

    if(!expand)
	return transcribe(p, 1 + p->namelen + 1, 1 + p->namelen + 1);

    e = FindEntityN(p->dtd, p->name, p->namelen, pe);
    if(!e)
    {
	Char *buf;
	Char *q;
	int i;

	if(pe || ParserGetFlag(p, ErrorOnUndefinedEntities))
	    return error(p, "Undefined%s entity %.*S",
			 pe ? " parameter" : "" ,
			 p->namelen > 50 ? 50 : p->namelen, p->name);

	warn(p, "Undefined%s entity %.*S",
	     pe ? " parameter" : "",
	     p->namelen > 50 ? 50 : p->namelen, p->name);

	/* Fake a definition for it */

	buf = Malloc((5 + p->namelen + 1 + 1) * sizeof(Char));
	if(!buf)
	    return error(p, "System error");
	q = buf;
	*q++ = '&'; *q++ = '#'; *q++ = '3'; *q++ = '8'; *q++ = ';';
	for(i=0; i<p->namelen; i++)
	    *q++ = p->name[i];
	*q++ = ';';
	*q++ = 0;

	if(!(e = NewInternalEntityN(p->name, p->namelen, buf, 0, 0, 0, 0)))
	    return error(p, "System error");
	if(!DefineEntity(p->dtd, e, 0))
	    return error(p, "System error");
    }

    if(!allow_external && e->type == ET_external)
	return error(p, "Illegal reference to external entity");

    for(s = p->source; s; s = s->parent)
	if(s->entity == e)
	    return error(p, "Recursive reference to entity \"%S\"", e->name);

    if(e->type == ET_external && p->entity_opener)
	s = p->entity_opener(e, p->callback_arg);
    else
	s = EntityOpen(e);
    if(!s)
	return error(p, "Couldn't open entity %S, %s",
		     e->name, EntityDescription(e));
    
    require(ParserPush(p, s));

    return 0;
}

static int parse_character_reference(Parser p, int expand)
{
    InputSource s = p->source;
    int c, base = 10;
    int count = 0;
    unsigned int code = 0;
    Char *ch = s->line + s->next;

    if(looking_at(p, "x"))
    {
	ch++;
	base = 16;
    }

    while((c = get(s)) != ';')
    {
	if((c >= '0' && c <= '9') ||
	   (base == 16 && ((c >= 'A' && c <= 'F') ||
			   (c >= 'a' && c <= 'f'))))
	    count++;
	else
	{
	    unget(s);		/* For error position */
	    return error(p, 
			 "Illegal character %s in base-%d character reference",
			 escape(c), base);
	}
    }

    if(!expand)
	return transcribe(p, 2 + (base == 16) + count + 1,
			     2 + (base == 16) + count + 1);

    while(count-- > 0)
    {
	c = *ch++;
	if(c >= '0' && c <= '9')
	    code = code * base + (c - '0');
	else if(c >= 'A' && c <= 'F')
	    code = code * base + 10 + (c - 'A');
	else
	    code = code * base + 10 + (c - 'a');
    }

#if CHAR_SIZE == 8
    if(code > 255 || !is_xml_legal(code))
    {
	if(ParserGetFlag(p, ErrorOnBadCharacterEntities))
	    return error(p, "0x%x is not a valid 8-bit XML character", code);
	else
	    warn(p, "0x%x is not a valid 8-bit XML character; ignored", code);
	return 0;
    }
#else
    if(!is_xml_legal(code))
    {
	if(ParserGetFlag(p, ErrorOnBadCharacterEntities))
	    return error(p, "0x%x is not a valid UTF-16 XML character", code);
	else
	    warn(p, "0x%x is not a valid UTF-16 XML character; ignored", code);
	return 0;
    }

    if(code >= 0x10000)
    {
	/* Use surrogates */

	ExpandBuf(p->pbuf, p->pbufnext+2);
	code -= 0x10000;

	p->pbuf[p->pbufnext++] = (code >> 10) + 0xd800;
	p->pbuf[p->pbufnext++] = (code & 0x3ff) + 0xdc00;

	return 0;
    }
#endif

    ExpandBuf(p->pbuf, p->pbufnext+1);
    p->pbuf[p->pbufnext++] = code;

    return 0;
}

/* Called after reading '<!ELEMENT ' */

static int parse_element_decl(Parser p)
{
    Char *name;
    ContentType type;
    ElementDefinition def;
#if 1
    ContentParticle cp;
#else
    int c;
    Char pcdata[] = {'#','P','C','D','A','T','A',0};
#endif
    Char *content = 0;

    require(parse_name(p, "for name in element declaration"));
    CopyName(name);
    maybe_uppercase(p, name);

    require(expect_dtd_whitespace(p, "after name in element declaration"));

    if(looking_at(p, "EMPTY"))
    {
	type = CT_empty;
	content = 0;
    }
    else if(looking_at(p, "ANY"))
    {
	type = CT_any;
	content = 0;
    }
    else
#if 1
	if(looking_at(p, "("))
    {
	unget(p->source);
	if(!(cp = parse_cp(p)) ||
	   check_content_decl(p, cp) < 0 ||
	   !(content = stringify_cp(cp)))
	{
	    FreeContentParticle(cp);
	    Free(content);
	    Free(name);
	    return -1;
	}

	if(cp->type == CP_choice && cp->children[0]->type == CP_pcdata)
	    type = CT_mixed;
	else
	    type = CT_element;
	{
	}
	FreeContentParticle(cp); /* XXX */
    }
    else
    {
	Free(name);
	return error(p, "Expected \"EMPTY\", \"ANY\", or \"(\" after name in "
		        "element declaration");
    }
#else
    {
	/* Don't really parse here... maybe improve sometime */

	int count = 0;

	p->pbufnext = 0;

	while((c = get(p->source)) != '>')
	{
	    switch(c)
	    {
	    case XEOE:
		if(count > 0)
		    require(transcribe(p, count, count));
		if(!p->source->parent)
		    return error(p, "EOE in element declaration");
		ParserPop(p);
		count = 0;
		break;
	    case '%':
		if(count > 0)
		    require(transcribe(p, count+1, count));
		if(p->external_pe_depth == 0)
		{
		    unget(p->source); /* For error position */
		    return error(p,
				 "PE ref not allowed here in internal subset");
		}
		require(parse_reference(p, 1, 1, 1));
		count = 0;
		break;
	    default:
		count++;
		if(at_eol(p->source))
		{
		    require(transcribe(p, count, count));
		    count = 0;
		}
	    }
	}

	unget(p->source);
	require(transcribe(p, count, count));
	p->pbuf[p->pbufnext++] = 0;

	if(Strstr(p->pbuf, pcdata))
	    type = CT_mixed;
	else
	    type = CT_element;
	
	content = p->pbuf;
	Consume(p->pbuf);
    }
#endif
    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
    require(expect(p, '>', "at end of element declaration"));

    if((def = FindElement(p->dtd, name)))
    {
	if(def->tentative)
	    RedefineElement(def, type, content);
	else
	{
	    Free(content);
	    if(ParserGetFlag(p, WarnOnRedefinitions))
		warn(p, "Ignoring redeclaration of element %S", name);
	}
    }
    else
	if (!DefineElement(p->dtd, name, type, content)) {
	    return error(p, "System error");
	};

    Free(name);

    return 0;
}

/* Content model parsing */

static ContentParticle parse_cp(Parser p)
{
    ContentParticle cp;

    if(looking_at(p, "("))
    {
	if(!(cp = parse_choice_or_seq(p)))
	    return 0;
    }
    else if(looking_at(p, "#PCDATA"))
    {
	if(!(cp = Malloc(sizeof(*cp))))
	{
	    error(p, "System error");
	    return 0;
	}
	
	cp->type = CP_pcdata;
    }
    else
    {
	if(parse_name(p, "in content declaration") < 0)
	    return 0;

	if(!(cp = Malloc(sizeof(*cp))))
	{
	    error(p, "System error");
	    return 0;
	}
	
	cp->type = CP_name;
	CopyName0(cp->name);
    }

    if(looking_at(p, "*"))
	cp->repetition = '*';
    else if(looking_at(p, "+"))
	cp->repetition = '+';
    else if(looking_at(p, "?"))
	cp->repetition = '?';
    else 
	cp->repetition = 0;

    return cp;
}

/* Called after '(' */

static ContentParticle parse_choice_or_seq(Parser p)
{
    ContentParticle cp, cp1;


    require0(skip_dtd_whitespace(p, p->external_pe_depth > 0));

    if(!(cp1 = parse_cp(p)))
	return 0;

    require0(skip_dtd_whitespace(p, p->external_pe_depth > 0));

    if(!(cp = parse_choice_or_seq_1(p, 1, 0)))
	FreeContentParticle(cp1);
    else
	cp->children[0] = cp1;

    return cp;
}

/* Called before '|', ',', or ')' */

static ContentParticle parse_choice_or_seq_1(Parser p, int nchildren, char sep)
{
    ContentParticle cp = 0, cp1;
    int nsep = get(p->source);

    if(nsep == ')')
    {
	/* We've reached the end */

	if(!(cp = Malloc(sizeof(*cp))) ||
	   !(cp->children = Malloc(nchildren * sizeof(cp))))
	{
	    Free(cp);
	    error(p, "System error");
	    return 0;
	}

	/* The standard does not specify whether '(foo)' is a choice or a
	   sequence.  We make it a choice so that (#PCDATA) comes out as
	   a choice, like other mixed models. */

	cp->type = sep == ',' ? CP_seq : CP_choice;
	cp->nchildren = nchildren;

	return cp;
    }

    if(nsep != '|' && nsep != ',')
    {
	error(p, "Expected | or , or ) in content declaration, got %s",
	      escape(nsep));
	return 0;
    }

    if(sep && nsep != sep)
    {
	error(p, "Content particle contains both | and ,");
	return 0;
    }

    require0(skip_dtd_whitespace(p, p->external_pe_depth > 0));

    if(!(cp1 = parse_cp(p)))
	return 0;

    require0(skip_dtd_whitespace(p, p->external_pe_depth > 0));

    if(!(cp = parse_choice_or_seq_1(p, nchildren+1, (char)nsep)))
	FreeContentParticle(cp1);
    else
	cp->children[nchildren] = cp1;

    return cp;
}

/* Check content particle matches Mixed or children */

static int check_content_decl(Parser p, ContentParticle cp)
{
    int i;

    if(cp->type == CP_choice && cp->children[0]->type == CP_pcdata)
    {
	for(i=1; i<cp->nchildren; i++)
	    if(cp->children[i]->type != CP_name)
		return error(p, "Invalid mixed content declaration");

	if(cp->repetition != '*' &&
	   !(cp->nchildren == 1 && cp->repetition == 0))
	    return error(p, "Invalid mixed content declaration");

	return 0;
    }
    else
	return check_content_decl_1(p, cp);
}

static int check_content_decl_1(Parser p, ContentParticle cp)
{
    int i;

    switch(cp->type)
    {
    case CP_pcdata:
	return error(p, "Misplaced #PCDATA in content declaration");
    case CP_seq:
    case CP_choice:
	for(i=0; i<cp->nchildren; i++)
	    if(check_content_decl_1(p, cp->children[i]) < 0)
		return -1;
	return 0;
    default:
	return 0;
    }
}

/* Reconstruct the content model as a string */

static Char *stringify_cp(ContentParticle cp)
{
    int size = size_cp(cp);
    Char *s;
    FILE16 *f;

    if(!(s = Malloc((size+1) * sizeof(Char))) || 
       !(f = MakeFILE16FromString(s, (size + 1) * sizeof(Char), "w")))
    {
	Free(s);
	return 0;
    }

    print_cp(cp, f);
    s[size] = 0;

    Fclose(f);

    return s;
}

static void print_cp(ContentParticle cp, FILE16 *f)
{
    int i;

    switch(cp->type)
    {
    case CP_pcdata:
	Fprintf(f, "#PCDATA");
	break;
    case CP_name:
	Fprintf(f, "%S", cp->name);
	break;
    case CP_seq:
    case CP_choice:
	Fprintf(f, "(");
	for(i=0; i<cp->nchildren; i++)
	{
	    if(i != 0)
		Fprintf(f, cp->type == CP_seq ? "," : "|");
	    print_cp(cp->children[i], f);
	}
	Fprintf(f, ")");
	break;
    }

    if(cp->repetition)
	Fprintf(f, "%c", cp->repetition);
}

static int size_cp(ContentParticle cp)
{
    int i, s;

    switch(cp->type)
    {
    case CP_pcdata:
	s = 7;
	break;
    case CP_name:
	s = Strlen(cp->name);
	break;
    default:
	s = 2;
	for(i=0; i<cp->nchildren; i++)
	{
	    if(i != 0)
		s++;
	    s += size_cp(cp->children[i]);
	}
	break;
    }

    if(cp->repetition)
	s++;

    return s;
}

void FreeContentParticle(ContentParticle cp)
{
    int i;

    if(!cp)
	return;

    switch(cp->type)
    {
    case CP_pcdata:
	break;
    case CP_name:
	Free(cp->name);
	break;
    case CP_seq:
    case CP_choice:
	for(i=0; i<cp->nchildren; i++)
	    FreeContentParticle(cp->children[i]);
	Free(cp->children);
	break;
    }

    Free(cp);
}

/* Called after reading '<!ATTLIST ' */

static int parse_attlist_decl(Parser p)
{
    Char *name;
    ElementDefinition element;
    AttributeType type;
    DefaultType default_type;
    Char **allowed_values, *t;
    Char *default_value;
    int nvalues, i;

    require(parse_name(p, "for name in attlist declaration"));
    CopyName(name);
    maybe_uppercase(p, name);

    if(!(element = FindElement(p->dtd, name)))
    {
	if(!(element = TentativelyDefineElement(p->dtd, name)))
	    return error(p, "System error");
    }
    Free(name);

    require(expect_dtd_whitespace(p,
			        "after element name in attlist declaration"));

    while(!looking_at(p, ">"))
    {
	require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	require(parse_name(p, "for attribute in attlist declaration"));
	CopyName(name);
	maybe_uppercase(p, name);
    
	require(expect_dtd_whitespace(p, "after name in attlist declaration"));

	if(looking_at(p, "CDATA"))
	    type = AT_cdata;
	else if(looking_at(p, "IDREFS"))
	    type = AT_idrefs;
	else if(looking_at(p, "IDREF"))
	    type = AT_idref;
	else if(looking_at(p, "ID"))
	    type = AT_id;
	else if(looking_at(p, "ENTITIES"))
	    type = AT_entities;
	else if(looking_at(p, "ENTITY"))
	    type = AT_entity;
	else if(looking_at(p, "NMTOKENS"))
	    type = AT_nmtokens;
	else if(looking_at(p, "NMTOKEN"))
	    type = AT_nmtoken;
	else if(looking_at(p, "NOTATION"))
	    type = AT_notation;
	else
	    type = AT_enumeration;

	if(type != AT_enumeration)
	{
	    require(expect_dtd_whitespace(p, "after attribute type"));
        }

	if(type == AT_notation || type == AT_enumeration)
	{
	    require(expect(p, '(', 
			   "or keyword for type in attlist declaration"));

	    nvalues = 0;
	    p->pbufnext = 0;
	    do
	    {
		require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
		if(type == AT_notation)
		    require(parse_name(p,
			       "for notation value in attlist declaration"));
		else
		    require(parse_nmtoken(p,
			       "for enumerated value in attlist declaration"));
		maybe_uppercase_name(p);
		ExpandBuf(p->pbuf, p->pbufnext + p->namelen + 1);
		memcpy(p->pbuf+p->pbufnext, 
		       p->name,
		       p->namelen * sizeof(Char));
		p->pbuf[p->pbufnext + p->namelen] = 0;
		p->pbufnext += (p->namelen + 1);
		nvalues++;
		require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	    }
	    while(looking_at(p, "|"));

	    require(expect(p, ')',
		  "at end of enumerated value list in attlist declaration"));
	    require(expect_dtd_whitespace(p, "after enumerated value list "
					     "in attlist declaration"));

	    allowed_values = Malloc((nvalues+1)*sizeof(Char *));
	    if(!allowed_values)
		return error(p, "System error");
	    for(i=0, t=p->pbuf; i<nvalues; i++)
	    {
		allowed_values[i] = t;
		while(*t++)
		    ;
	    }
	    allowed_values[nvalues] = 0;

	    Consume(p->pbuf);
	}
	else
	    allowed_values = 0;

	if(looking_at(p, "#REQUIRED"))
	    default_type = DT_required;
	else if(looking_at(p, "#IMPLIED"))
	    default_type = DT_implied;
	else if(looking_at(p, "#FIXED"))
	{
	    default_type = DT_fixed;
	    require(expect_dtd_whitespace(p, "after #FIXED"));
	}
	else
	    default_type = DT_none;

	if(default_type == DT_fixed || default_type == DT_none)
	{
	    require(parse_string(p,
				 "for default value in attlist declaration",
				 type == AT_cdata ? LT_cdata_attr :
				                    LT_tok_attr));
	    default_value = p->pbuf;
	    Consume(p->pbuf);
	    if(type != AT_cdata && type != AT_entity && type != AT_entities)
		maybe_uppercase(p, default_value);
	}
	else
	    default_value = 0;

	require(skip_dtd_whitespace(p, p->external_pe_depth > 0));

	if(FindAttribute(element, name))
	{
	    if(ParserGetFlag(p, WarnOnRedefinitions))
		warn(p, "Ignoring redeclaration of attribute %S", name);
	    if(allowed_values)
	    {
		Free(allowed_values[0]);
		Free(allowed_values);
	    }
	    if(default_value)
		Free(default_value);
	}
	else
	    if(!DefineAttribute(element, name, type, allowed_values,
			    default_type, default_value))
		return error(p, "System error");

	Free(name);
    }
    
    return 0;
}

/* Used for external dtd part, entity definitions and notation definitions. */
/* NB PE references are not allowed here (why not?) */

static int parse_external_id(Parser p, int required,
			     char8 **publicid, char8 **systemid,
			     int preq, int sreq)
{
    InputSource s = p->source;
    int c;
    Char *cp;

    *publicid = 0;
    *systemid = 0;

    if(looking_at(p, "SYSTEM"))
    {
	if(!sreq)
	{
	    skip_whitespace(s);
	    c = get(s); unget(s);
	    if(c != '"' && c != '\'')
		return 0;
	}
	else
	    require(expect_dtd_whitespace(p, "after SYSTEM"));

	require(parse_string(p, "for system ID", LT_plain));
	if(!(*systemid = strdup8(Chartochar8(p->pbuf))))
	    return error(p, "System error");
    }
    else if(looking_at(p, "PUBLIC"))
    {
	if(!preq && !sreq)
	{
	    skip_whitespace(s);
	    c = get(s); unget(s);
	    if(c != '"' && c != '\'')
		return 0;
	}
	else
	    require(expect_dtd_whitespace(p, "after PUBLIC"));

	require(parse_string(p, "for public ID", LT_plain));

	for(cp=p->pbuf; *cp; cp++)
	    if(!is_ascii_alpha(*cp) && !is_ascii_digit(*cp) &&
	       strchr8("-'()+,./:=?;!*#@$_% \r\n", *cp) == 0)
		return error(p, "Illegal character %s in public id",
			     escape(*cp));

	if(!(*publicid = strdup8(Chartochar8(p->pbuf))))
	    return error(p, "System error");

	if(!sreq)
	{
	    skip_whitespace(s);
	    c = get(s); unget(s);
	    if(c != '"' && c != '\'')
		return 0;
	}
	else
	    require(expect_dtd_whitespace(p, "after public id"));

	require(parse_string(p, "for system ID", LT_plain));
	if(!(*systemid = strdup8(Chartochar8(p->pbuf))))
	    return error(p, "System error");
    }
    else if(required)
	return error(p, "Missing or invalid external ID");

    return 0;
}

/* Called after reading '<!ENTITY ' */

static int parse_entity_decl(Parser p, Entity ent, int line, int chpos)
{
    Entity e, old;
    int pe, t;
    Char *name;

    pe = looking_at(p, "%");	/* If it were a PE ref, we would
				   already have pushed it */

    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
    require(parse_name(p, "for name in entity declaration"));
    CopyName(name);

    require(expect_dtd_whitespace(p, "after name in entity declaration"));

    if(looking_at(p, "'") || looking_at(p, "\""))
    {
	Char *value;

	unget(p->source);
	require(parse_string(p, "for value in entity declaration", LT_entity));
	value = p->pbuf;
	Consume(p->pbuf);

	if(!(e = NewInternalEntity(name, value, ent, line, chpos, 0)))
	    return error(p, "System error");
    }
    else
    {
	char8 *publicid, *systemid;
	NotationDefinition notation = 0;

	require(parse_external_id(p, 1, &publicid, &systemid, 1, 1));

	require((t = skip_dtd_whitespace(p, p->external_pe_depth > 0)));
	if(looking_at(p, "NDATA"))
	{
	    if(t == 0)
		return error(p, "Whitespace missing before NDATA");
	    if(pe)
		return error(p, "NDATA not allowed for parameter entity");
	    require(expect_dtd_whitespace(p, "after NDATA"));
	    require(parse_name(p, "for notation name in entity declaration"));
	    maybe_uppercase_name(p);
	    notation = FindNotationN(p->dtd, p->name, p->namelen);
	    if(!notation)
	    {
		notation =
		    TentativelyDefineNotationN(p->dtd, p->name, p->namelen);
		if(!notation)
		    return error(p, "System error");
	    }
	}

	if(!(e = NewExternalEntity(name, publicid, systemid, notation, ent)))
	    return error(p, "System error");
    }

    Free(name);

    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
    require(expect(p, '>', "at end of entity declaration"));

    if((old = FindEntity(p->dtd, e->name, pe)) &&
       old->parent != xml_builtin_entity)
    {
	if(ParserGetFlag(p, WarnOnRedefinitions))
	    warn(p, "Ignoring redefinition of%s entity %S",
		 pe ? " parameter" : "", e->name);
    }
    else
	if(!DefineEntity(p->dtd, e, pe))
	    return error(p, "System error");

    return 0;
}

/* Called after reading '<!NOTATION ' */

static int parse_notation_decl(Parser p)
{
    Char *name;
    char8 *publicid, *systemid;
    NotationDefinition def;

    require(parse_name(p, "for name in notation declaration"));
    CopyName(name);
    maybe_uppercase(p, name);

    require(expect_dtd_whitespace(p, "after name in notation declaration"));
    
    require(parse_external_id(p, 1, &publicid, &systemid, 1, 0));

    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
    require(expect(p, '>', "at end of notation declaration"));

    if((def = FindNotation(p->dtd, name)))
    {
	if(def->tentative)
	    RedefineNotation(def, publicid, systemid);
	else
	    if(ParserGetFlag(p, WarnOnRedefinitions))
	    {
		warn(p, "Ignoring redefinition of notation %S", name);
		if(publicid) Free(publicid);
		if(systemid) Free(systemid);
	    }
    }
    else
    {
	if(!DefineNotation(p->dtd, name, publicid, systemid))
	    return error(p, "System error");
    }

    Free(name);

    return 0;
}

static int parse_conditional(Parser p)
{
    int depth=1;

    if(p->external_pe_depth == 0)
	return error(p, "Conditional section not allowed in internal subset");

    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
    if(looking_at(p, "INCLUDE"))
    {
	require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	require(expect(p, '[', "at start of conditional section"));
	require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	while(!looking_at(p, "]"))
	{
	    switch(parse_markupdecl(p))
	    {
	    case 1:
		return error(p, "EOF in conditional section");
	    case -1:
		return -1;
	    }
	    require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	}

	if(!looking_at(p, "]>"))
	    return error(p, "]> required after ] in conditional section");
    }
    else if(looking_at(p, "IGNORE"))
    {
	/* Easy, because ]]> not even allowed in strings! */

	require(skip_dtd_whitespace(p, p->external_pe_depth > 0));
	require(expect(p, '[', "at start of conditional section"));

	while(depth > 0)
	{
	    switch(get(p->source))
	    {
	    case XEOE:
		if(p->source->parent)
		    ParserPop(p);
		else
		    return error(p, "EOE in ignored conditional section");
		break;
	    case '<':
		if(looking_at(p, "!["))
		    depth++;
		break;
	    case ']':
		if(looking_at(p, "]>"))
		    depth--;
	    }
	}
    }
    else
	return error(p, "INCLUDE or IGNORE required in conditional section");

    return 0;
}

static void maybe_uppercase(Parser p, Char *s)
{
    if(ParserGetFlag(p, CaseInsensitive))
	while(*s)
	{
	    *s = Toupper(*s);
	    s++;
	}
}

static void maybe_uppercase_name(Parser p)
{
    int i;

    if(ParserGetFlag(p, CaseInsensitive))
	for(i=0; i<p->namelen; i++)
	    p->name[i] = Toupper(p->name[i]);
}

static int str_maybecase_cmp8(Parser p, const char8 *a, const char8 *b)
{
    return
	ParserGetFlag(p, CaseInsensitive) ? strcasecmp8(a, b) : strcmp8(a, b);
}

static int is_ascii_alpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_ascii_digit(int c)
{
    return c >= '0' && c <= '9';
}

/* Error handling */

static void verror(XBit bit, const char8 *format, va_list args)
{
    /* yuk, but we don't want to fail if we can't allocate */
    static char8 message[400];

    /* Print message before freeing xbit, so we can print data from it */
    Vsprintf(message, CE_ISO_8859_1, format, args);

    FreeXBit(bit);
    bit->type = XBIT_error;
    bit->error_message = message;
}

static int error(Parser p, const char8 *format, ...)
{
    va_list args;

    va_start(args, format);
    verror(&p->xbit, format, args);

    p->state = PS_error;

    return -1;
}

static void warn(Parser p, const char8 *format, ...)
{
    va_list args;
    static struct xbit bit;

    va_start(args, format);
    verror(&bit, format, args);

    bit.type = XBIT_warning;

    if(p->warning_callback)
	p->warning_callback(&bit, p->callback_arg);
    else
	ParserPerror(p, &bit);
}

