#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "charset.h"
#include "string16.h"
#include "dtd.h"
#include "input.h"
#include "xmlparser.h"
#include "stdio16.h"

int attr_compare(const void *a, const void *b);
void print_tree(Parser p, XBit bit);
void print_bit(Parser p, XBit bit);
void print_attrs(ElementDefinition e, Attribute a);
void print_text(Char *text);
void print_text_bit(Char *text);
void dtd_cb(XBit bit, void *arg);
InputSource entity_open(Entity ent, void *arg);

int verbose = 0, expand = 0, bits = 0, silent = 0, nsgml = 0,
    attr_defaults = 0, merge = 0, strict_xml = 0, tree = 0;
char *enc_name = 0;
CharacterEncoding encoding = CE_unknown;
InputSource source = 0;

int main(int argc, char **argv)
{
    int i;
    Parser p;
    char *s;
    Entity ent = 0;

    /* Sigh... guess which well-known system doesn't have getopt() */

    for(i = 1; i < argc; i++)
    {
	if(argv[i][0] != '-')
	    break;
	for(s = &argv[i][1]; *s; s++)
	    switch(*s)
	    {
	    case 'v':
		verbose = 1;
		break;
	    case 'a':
		attr_defaults = 1;
		break;
	    case 'e':
		expand = 1;
		break;
	    case 'b':
		bits = 1;
		break;
	    case 's':
		silent = 1;
		break;
	    case 'n':
		nsgml = 1;
		break;
	    case 'c':
		enc_name = argv[++i];
		break;
	    case 'm':
		merge = 1;
		break;
	    case 't':
		tree = 1;
		break;
	    case 'x':
		strict_xml = 1;
		attr_defaults = 1;
		expand = 1;
		break;
	    default:
		fprintf(stderr, 
			"usage: rxp [-abemnstvx] [-c encoding] [url]\n");
		return 1;
	    }
    }

    if(i < argc)
    {
	ent = NewExternalEntity(0, 0, strdup8(argv[i]), 0, 0);
	if(ent)
	    source = EntityOpen(ent);
    }
    else
	source = SourceFromStream("<stdin>", stdin);

    if(!source)
	return 1;

    p = NewParser();
    ParserSetEntityOpener(p, entity_open);

    if(bits)
    {
	ParserSetDtdCallback(p, dtd_cb);
	ParserSetCallbackArg(p, p);
    }

    if(attr_defaults)
	ParserSetFlag(p, ReturnDefaultedAttributes, 1);

    if(!expand)
    {
	ParserSetFlag(p, ExpandGeneralEntities, 0);
	ParserSetFlag(p, ExpandCharacterEntities, 0);
    }

    if(merge)
	ParserSetFlag(p, MergePCData, 1);

    if(nsgml)
    {
	ParserSetFlag(p, XMLPiEnd, 0);
	ParserSetFlag(p, XMLEmptyTagEnd, 0);
	ParserSetFlag(p, XMLPredefinedEntities, 0);
	ParserSetFlag(p, XMLExternalIDs, 0);
	ParserSetFlag(p, XMLMiscWFErrors, 0);
	ParserSetFlag(p, TrustSDD, 0);
	ParserSetFlag(p, ErrorOnUnquotedAttributeValues, 0);
	ParserSetFlag(p, ExpandGeneralEntities, 0);
	ParserSetFlag(p, ExpandCharacterEntities, 0);
/*	ParserSetFlag(p, TrimPCData, 1); */
    }

    if(strict_xml)
    {
	ParserSetFlag(p, ErrorOnBadCharacterEntities, 1);	
	ParserSetFlag(p, ErrorOnUndefinedEntities, 1);
	ParserSetFlag(p, XMLStrictWFErrors, 1);
	ParserSetFlag(p, WarnOnUndefinedElements, 0);
	ParserSetFlag(p, WarnOnUndefinedAttributes, 0);
	ParserSetFlag(p, WarnOnRedefinitions, 0);
	
    }

    if(ParserPush(p, source) == -1)
    {
	ParserPerror(p, &p->xbit);
	return 1;
    }

    if(enc_name)
    {
	encoding = FindEncoding(enc_name);

	if(encoding == CE_unknown)
	{
	    fprintf(stderr, "unknown encoding %s\n", enc_name);
	    return 1;
	}
    }
    else if(strict_xml)
	encoding = CE_UTF_8;
    else
	encoding = source->entity->encoding;

    SetFileEncoding(Stdout, encoding);

    if(verbose)
	fprintf(stderr, "Input encoding %s, output encoding %s\n",
		CharacterEncodingNameAndByteOrder[source->entity->encoding],
		CharacterEncodingNameAndByteOrder[encoding]);

    if(!silent && !strict_xml && source->entity->ml_decl == ML_xml && !bits)
    {
	Printf("<?xml");

	if(source->entity->version_decl)
	    Printf(" version=\"%s\"", source->entity->version_decl);

	if(encoding == CE_unspecified_ascii_superset)
	{
	    if(source->entity->encoding_decl != CE_unknown)
		Printf(" encoding=\"%s\"", 
		       CharacterEncodingName[source->entity->encoding_decl]);
	}
	else
	    Printf(" encoding=\"%s\"",
		   CharacterEncodingName[encoding]);

	if(source->entity->standalone_decl != SDD_unspecified)
	    Printf(" standalone=\"%s\"", 
		   StandaloneDeclarationName[source->entity->standalone_decl]);

	Printf("?>\n");
    }

    while(1)
    {
	XBit bit;

	if(tree)
	{
	    bit = ReadXTree(p);
	    print_tree(p, bit);
	}
	else
	{
	    bit = ReadXBit(p);
	    print_bit(p, bit);
	}
	if(bit->type == XBIT_eof)
	{
	    if(!silent && !strict_xml && !bits)
		Printf("\n");

	    /* Not necessary, but helps me check for leaks */
	    FreeDtd(p->dtd);
	    FreeParser(p);
	    if(ent)
		FreeEntity(ent);
	    return 0;
	}
	if(bit->type == XBIT_error)
	    return 1;
	if(tree)
	    FreeXTree(bit);
	else
	    FreeXBit(bit);
    }
}

void print_tree(Parser p, XBit bit)
{
    int i;
    struct xbit endbit;

    print_bit(p, bit);
    if(bit->type == XBIT_start)
    {
	for(i=0; i<bit->nchildren; i++)
	    print_tree(p, bit->children[i]);
	endbit.type = XBIT_end;
	endbit.element_definition = bit->element_definition;
	print_bit(p, &endbit);
    }
}

void print_bit(Parser p, XBit bit)
{
    const char *sys, *pub;

    if(silent && bit->type != XBIT_error)
	return;

    if(bits)
    {
	Printf("At %d: ", bit->byte_offset);
	switch(bit->type)
	{
	case XBIT_eof:
	    Printf("EOF\n");
	    break;
	case XBIT_error:
	    ParserPerror(p, bit);
	    break;
	case XBIT_dtd:
	    sys = pub = "<none>";
	    if(p->dtd->external_part)
	    {
		if(p->dtd->external_part->publicid)
		    pub = p->dtd->external_part->publicid;
		if(p->dtd->external_part->systemid)
		    sys = p->dtd->external_part->systemid;
	    }
	    Printf("doctype: %S pubid %s sysid %s\n", p->dtd->name, pub, sys);
	    break;
	case XBIT_start:
	    Printf("start: %S ", bit->element_definition->name);
	    print_attrs(0, bit->attributes);
	    Printf("\n");
	    break;
	case XBIT_empty:
	    Printf("empty: %S ", bit->element_definition->name);
	    print_attrs(0, bit->attributes);
	    Printf("\n");
	    break;
	case XBIT_end:
	    Printf("end: %S\n", bit->element_definition->name);
	    break;
	case XBIT_pi:
	    Printf("pi: %S: ", bit->pi_name);
	    print_text_bit(bit->pi_chars);
	    Printf("\n");
	    break;
	case XBIT_cdsect:
	    Printf("cdata: ");
	    print_text_bit(bit->cdsect_chars);
	    Printf("\n");
	    break;
	case XBIT_pcdata:
	    Printf("pcdata: ");
	    print_text_bit(bit->pcdata_chars);
	    Printf("\n");
	    break;
	case XBIT_comment:
	    Printf("comment: ");
	    print_text_bit(bit->comment_chars);
	    Printf("\n");
	    break;
	default:
	    fprintf(stderr, "***%s\n", XBitTypeName[bit->type]);
	    exit(1);
	    break;
	}
    }
    else
    {
	switch(bit->type)
	{
	case XBIT_eof:
	    break;
	case XBIT_error:
	    ParserPerror(p, bit);
	    break;
	case XBIT_dtd:
	    if(strict_xml)
		/* no doctype in canonical XML */
		break;
	    Printf("<!DOCTYPE %S", p->dtd->name);
	    if(p->dtd->external_part)
	    {
		if(p->dtd->external_part->publicid)
		    Printf(" PUBLIC \"%s\"", p->dtd->external_part->publicid);
		else if(p->dtd->external_part->systemid)
		    Printf(" SYSTEM");
		if(p->dtd->external_part->systemid)
		    Printf(" \"%s\"", p->dtd->external_part->systemid);
	    }
	    if(p->dtd->internal_part)
		Printf(" [%S]", p->dtd->internal_part->text);
	    Printf(">\n");
	    break;
	case XBIT_start:
	case XBIT_empty:
	    Printf("<%S", bit->element_definition->name);
	    print_attrs(bit->element_definition, bit->attributes);
	    if(bit->type == XBIT_start)
		Printf(">");
	    else if(strict_xml)
		Printf("></%S>", bit->element_definition->name);
	    else
		Printf("/>");
	    break;
	case XBIT_end:
	    Printf("</%S>", bit->element_definition->name);
	    break;
	case XBIT_pi:
	    Printf("<?%S %S%s", 
		   bit->pi_name, bit->pi_chars, nsgml ? ">" : "?>");
	    if(p->state <= PS_prolog2 && !strict_xml)
		Printf("\n");
	    break;
	case XBIT_cdsect:
	    if(strict_xml)
		/* Print CDATA sections as plain PCDATA in canonical XML */
		print_text(bit->cdsect_chars);
	    else
		Printf("<![CDATA[%S]]>", bit->cdsect_chars);
	    break;
	case XBIT_pcdata:
	    print_text(bit->pcdata_chars);
	    break;
	case XBIT_comment:
	    if(strict_xml)
		/* no comments in canonical XML */
		break;
	    Printf("<!--%S-->", bit->comment_chars);
	    if(p->state <= PS_prolog2)
		Printf("\n");
	    break;
	default:
	    fprintf(stderr, "\n***%s\n", XBitTypeName[bit->type]);
	    exit(1);
	    break;
	}
    }
}

int attr_compare(const void *a, const void *b)
{
    return Strcmp((*(Attribute *)a)->definition->name,
		  (*(Attribute *)b)->definition->name);
}

void print_attrs(ElementDefinition e, Attribute a)
{
    Attribute b;
    Attribute *aa;
    int i, n = 0;
    
    for(b=a; b; b=b->next)
	n++;

    if(n == 0)
	return;

    aa = malloc(n * sizeof(*aa));

    for(i=0, b=a; b; i++, b=b->next)
	aa[i] = b;

    if(strict_xml)
	qsort((void *)aa, n, sizeof(*aa), attr_compare);

    for(i=0; i<n; i++)
    {
	Printf(" %S=\"", aa[i]->definition->name);
	print_text(aa[i]->value);
	Printf("\"");
    }

    free(aa);
}

void print_text_bit(Char *text)
{
    int i;

    for(i=0; i<50 && text[i]; i++)
	if(text[i] == '\n' || text[i] == '\r')
	    text[i] = '~';
    Printf("%.50S", text);
}

void dtd_cb(XBit bit, void *arg)
{
    Printf("In DTD: ");
    print_bit(arg, bit);
}
	
void print_text(Char *text)
{
    Char *pc, *last;
    
    if(bits)
    {
	Printf("%S", text);
	return;
    }

    for(pc = last = text; *pc; pc++)
    {
	if(*pc == '&' || *pc == '<' || *pc == '>' || *pc == '"' ||
	   (strict_xml && (*pc == 9 || *pc == 10 || *pc == 13)))
	{
	    if(pc > last)
		Printf("%.*S", pc - last, last);
	    switch(*pc)
	    {
	    case '<':
		Printf("&lt;");
		break;
	    case '>':
		Printf("&gt;");
		break;
	    case '&':
		Printf("&amp;");
		break;
	    case '"':
		Printf("&quot;");
		break;
	    case 9:
		Printf("&#9;");
		break;
	    case 10:
		Printf("&#10;");
		break;
	    case 13:
		Printf("&#13;");
		break;
	    }
	    last = pc+1;
	}
    }
	
    if(pc > last)
	Printf("%.*S", pc - last, last);
}

InputSource entity_open(Entity ent, void *arg)
{
    if(ent->publicid && 
       strcmp(ent->publicid, "-//RMT//DTD just a test//EN") == 0)
    {
	FILE *f;
	FILE16 *f16;

	if((f = fopen("/tmp/mydtd", "r")))
	{
	    if(!(f16 = MakeFILE16FromFILE(f, "r")))
		return 0;
	    SetCloseUnderlying(f16, 1);

	    return NewInputSource(ent, f16);
	}
    }

    return EntityOpen(ent);
}

