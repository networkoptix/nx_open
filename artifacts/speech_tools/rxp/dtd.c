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
#include <string.h>

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
#include "dtd.h"
#include "url.h"

const char8 *DefaultTypeName[DT_enum_count] = {
    "#required",
    "bogus1",
    "#implied",
    "bogus2"
    "none",
    "#fixed",
};

const char8 *ContentTypeName[CT_enum_count] = {
    "mixed",
    "any",
    "bogus1",
    "bogus2",
    "empty",
    "element"
};

const char8 *AttributeTypeName[AT_enum_count] = {
    "cdata",
    "bogus1",
    "bogus2",
    "nmtoken",
    "bogus3",
    "entity",
    "idref",
    "bogus4",
    "bogus5",
    "nmtokens",
    "bogus6",
    "entities",
    "idrefs",
    "id",
    "notation",
    "enumeration"
};

const char8 *StandaloneDeclarationName[SDD_enum_count] = {
    "unspecified", "no", "yes"
};

static Char *Strndup(const Char *old, int len)
{
    Char *new = Malloc((len+1) * sizeof(Char));

    if(!new)
	return 0;

    memcpy(new, old, len * sizeof(Char));
    new[len] = 0;

    return new;
}

/* DTDs */

Dtd NewDtd(void)
{
    Dtd d;

    if(!(d = Malloc(sizeof(*d))))
	return 0;

    d->name = 0;
    d->internal_part = 0;
    d->external_part = 0;
    d->entities = 0;
    d->parameter_entities = 0;
    d->predefined_entities = 0;
#ifdef FOR_LT
    d->doctype = 0;
#else
    d->elements = 0;
#endif
    d->notations = 0;

    return d;
}

/* Free a DTD and everything in it */

void FreeDtd(Dtd dtd)
{
    Entity ent, ent1;
#ifndef FOR_LT
    ElementDefinition elem, elem1;
#endif
    NotationDefinition not, not1;

    if(!dtd)
	return;

    /* Free the name */

    Free((Char *)dtd->name);	/* cast is to get rid of const */

    /* Free the entities */

    FreeEntity(dtd->internal_part);
    FreeEntity(dtd->external_part);

    for(ent = dtd->entities; ent; ent = ent1)
    {
	ent1 = ent->next;	/* get it before we free ent! */
	FreeEntity(ent);
    }

    for(ent = dtd->parameter_entities; ent; ent = ent1)
    {
	ent1 = ent->next;
	FreeEntity(ent);
    }

    /* The predefined entities are shared, so we don't free them */

#ifndef FOR_LT
    /* Free the elements (kept elsewhere in NSL) */

    for(elem = dtd->elements; elem; elem = elem1)
    {
	elem1 = elem->next;
	FreeElementDefinition(elem); /* Frees the attributes too */
    }
#endif

    /* Free the notations */

    for(not = dtd->notations; not; not = not1)
    {
	not1 = not->next;
	FreeNotationDefinition(not);
    }

    /* Free the dtd itself */

    Free(dtd);
}

/* Entities */

/* 
 * Make a new entity.  The name is copied, none of the other
 * arguments are, but they are freed when the entity is freed!
 */

Entity NewExternalEntityN(const Char *name, int namelen, const char8 *publicid,
			  const char8 *systemid, NotationDefinition notation,
			  Entity parent)
{
    Entity e;

    if(!(e = Malloc(sizeof(*e))))
	return 0;
    if(name && !(name = Strndup(name, namelen)))
	    return 0;

    e->type = ET_external;
    e->name = name;
    e->base_url = 0;
    e->encoding = CE_unknown;
    e->next = 0;
    e->parent = parent;

    e->publicid = publicid;
    e->systemid = systemid;
    e->notation = notation;

    e->version_decl = 0;
    e->encoding_decl = CE_unknown;
    e->standalone_decl = SDD_unspecified;
    e->ddb_filename = 0;

    e->url = 0;

    return (Entity)e;
}

Entity NewInternalEntityN(const Char *name, int namelen,
			  const Char *text, Entity parent,
			  int line_offset, int line1_char_offset, 
			  int matches_parent_text)
{
    Entity e;

    if(!(e = Malloc(sizeof(*e))))
	return 0;
    if(name)
	if(!(name = Strndup(name, namelen)))
	    return 0;

    e->type = ET_internal;
    e->name = name;
    e->base_url = 0;
    e->encoding = InternalCharacterEncoding;
    e->next = 0;
    e->parent = parent;

    e->text = text;
    e->line_offset = line_offset;
    e->line1_char_offset = line1_char_offset;
    e->matches_parent_text = matches_parent_text;

    e->url = 0;

    return (Entity)e;
}

void FreeEntity(Entity e)
{
    if(!e)
	return;

    Free((void *)e->name);	/* The casts are to get rid of the const */
    Free((void *)e->base_url);
    Free((void *)e->url);
    
    switch(e->type)
    {
    case ET_internal:
	Free((void *)e->text);
	break;
    case ET_external:
	Free((void *)e->systemid);
	Free((void *)e->publicid);
	Free((void *)e->version_decl);
	Free((void *)e->ddb_filename);
	break;
    }

    Free(e);
}

const char8 *EntityURL(Entity e)
{
    /* Have we already determined the URL? */

    if(e->url)
	return e->url;

    if(e->type == ET_internal)
    {
	if(e->parent)
	{
	    const char8 *url = EntityURL(e->parent);
	    if(url)
		e->url = strdup8(url);
	}
    }
    else
	e->url = url_merge(e->systemid, 
			   e->parent ? EntityBaseURL(e->parent) : 0, 
			   0, 0, 0, 0);

    return e->url;
}

/* Returns the URL of the entity if it has one, otherwise the system ID.
   It will certainly have a URL if it was successfully opened by EntityOpen.
   Intended for error messages, so never returns NULL. */

const char8 *EntityDescription(Entity e)
{
    if(e->url)
	return e->url;

    if(e->type == ET_external)
	return e->systemid;

    if(e->parent)
	return EntityDescription(e->parent);

    return "<unknown>";
}

void EntitySetBaseURL(Entity e, const char8 *url)
{
    e->base_url = url;
}

const char8 *EntityBaseURL(Entity e)
{
    if(e->base_url)
	return e->base_url;

    if(e->type == ET_internal)
    {
	if(e->parent)
	    return EntityBaseURL(e->parent);
	else
	    return 0;
    }

    return EntityURL(e);
}

Entity DefineEntity(Dtd dtd, Entity e, int pe)
{
    if(pe)
    {
	e->next = dtd->parameter_entities;
	dtd->parameter_entities = e;
    }
    else
    {
	e->next = dtd->entities;
	dtd->entities = e;
    }

    return e;
}

Entity FindEntityN(Dtd dtd, const Char *name, int namelen, int pe)
{
    Entity e;

    if(!pe)
	for(e = dtd->predefined_entities; e; e = e->next)
	    if(Strncmp(name, e->name, namelen) == 0 && e->name[namelen] == 0)
		return e;


    for(e = pe ? dtd->parameter_entities : dtd->entities; e; e=e->next)
	if(Strncmp(name, e->name, namelen) == 0 && e->name[namelen] == 0)
	    return e;

    return 0;
}

/* Elements */

/* 
 * Define a new element.  The name is copied, the content model is not,
 * but it is freed when the element definition is freed!
 */

ElementDefinition DefineElementN(Dtd dtd, const Char *name, int namelen, 
				 ContentType type, Char *content)
{
#ifdef FOR_LT
    static struct element_definition e;
    RHTEntry *entry;
    struct element_content *c;

    if (!(entry = DeclareElement(dtd->doctype, name, namelen, 0,
				 (ctVals)type))) {
      return NULL;
    };

    e.doctype = dtd->doctype;
    e.name = (Char *)dtd->doctype->elements+entry->keyptr;
    e.elsum = (NSL_ElementSummary_I *)(dtd->doctype->permanentBase+entry->eval);

    if(type == CT_element || type == CT_mixed)
    {
	if(!(c = Malloc(sizeof(*c))))
	    return 0;
	c->elsum = e.elsum;
	c->content = content;
	c->next = e.doctype->element_content;
	e.doctype->element_content = c;
    }

    return &e;
#else
    ElementDefinition e;

    if(!(e = Malloc(sizeof(*e))) || !(name = Strndup(name, namelen)))
	return 0;

    e->tentative = 0;
    e->name = name;
    e->namelen = namelen;
    e->type = type;
    e->content = content;
    e->attributes = 0;
    e->next = dtd->elements;
    dtd->elements = e;

    return e;
#endif
}

ElementDefinition TentativelyDefineElementN(Dtd dtd, 
					    const Char *name, int namelen)
{
#ifdef FOR_LT
    static struct element_definition e;
    RHTEntry *entry;

    if (!(entry = DeclareElement(dtd->doctype, name, namelen, 0, (ctVals)CT_any))) {
      return NULL;
    };
    e.doctype = dtd->doctype;
    e.name = (Char *)dtd->doctype->elements+entry->keyptr;
    e.elsum = (NSL_ElementSummary_I *)(dtd->doctype->permanentBase+entry->eval);
    /* XXX use the omitStart field to note that it's tentative. */
    e.elsum->omitStart |= 2;
    return &e;
#else
    ElementDefinition e;

    if(!(e = Malloc(sizeof(*e))) || !(name = Strndup(name, namelen)))
	return 0;

    e->tentative = 1;
    e->name = name;
    e->namelen = namelen;
    e->type = CT_any;
    e->content = 0;
    e->attributes = 0;
    e->next = dtd->elements;
    dtd->elements = e;

    return e;
#endif
}

ElementDefinition RedefineElement(ElementDefinition e, ContentType type,
				  Char *content)
{
#ifdef FOR_LT
    struct element_content *c;

    e->elsum->contentType = type;
    e->elsum->omitStart &= ~2;
    if(type == CT_element)
    {
	if(!(c = Malloc(sizeof(*c))))
	    return 0;
	c->elsum = e->elsum;
	c->content = content;
	c->next = e->doctype->element_content;
	e->doctype->element_content = c;
    }
#else
    e->tentative = 0;
    e->type = type;
    e->content = content;
#endif
    return e;
}

ElementDefinition FindElementN(Dtd dtd, const Char *name, int namelen)
{
#ifdef FOR_LT
    /* Derived from FindElementAndName */

    static struct element_definition e;
    RHTEntry *entry;
    NSL_ElementSummary_I *elsum;

    if(!dtd->doctype)
	return 0;

    entry = rsearch(name, namelen, dtd->doctype->elements);
    if(!entry)
	return 0;

    elsum = (NSL_ElementSummary_I *)(dtd->doctype->permanentBase+entry->eval);

    e.doctype = dtd->doctype;
    e.name = (Char *)dtd->doctype->elements+entry->keyptr;
#if 0
    /* We don't use this so don't waste time on it XXX */
    e.namelen = Strlen(e.name);
#endif
    e.elsum = elsum;
    e.tentative = ((e.elsum->omitStart & 2) != 0);

    return &e;
#else
    ElementDefinition e, *p;

    for(p = &dtd->elements, e = *p; e; p = &e->next, e = *p)
	if(namelen == e->namelen && *name == *e->name &&
	   memcmp(name, e->name, namelen*sizeof(Char)) == 0)
	{
	    *p = e->next;
	    e->next = dtd->elements;
	    dtd->elements = e;
	    return e;
	}

    return 0;
#endif
}

/* Free an element definition and its attribute definitions */

void FreeElementDefinition(ElementDefinition e)
{
#ifndef FOR_LT
    AttributeDefinition a, a1;
#endif

    if(!e)
	return;

    /* Free the element name */

    Free((void *)e->name);

#ifndef FOR_LT
    /* Free the content model (kept elsewhere in NSL) */

    Free(e->content);

    /* Free the attributes (kept elsewhere in NSL) */

    for(a = e->attributes; a; a = a1)
    {
	a1 = a->next;
	FreeAttributeDefinition(a);
    }

    /* Free the ElementDefinition itself */
#endif

    Free(e);
}

/* Attributes */

/* 
 * Define a new attribute.  The name is copied, the allowed values and
 * default are not, but they are freed when the attribute definition is freed!
 */

AttributeDefinition
    DefineAttributeN(ElementDefinition element, const Char *name, int namelen,
		     AttributeType type, Char **allowed_values,
		     DefaultType default_type, const Char *default_value)
{
#ifdef FOR_LT
    int nav = 0;
    Char *av = allowed_values ? allowed_values[0] : 0;
    Char **avp;
    NSL_Doctype_I *doctype = element->doctype;

    if(!doctype)
	return 0;

    if(allowed_values)
    {
	for(avp = allowed_values; *avp; avp++)
	    nav++;
	Free(allowed_values);
    }

    if (!(name = DeclareAttr(doctype, name, namelen,
		       (NSL_Attr_Declared_Value)type,
		       av, nav,
		       (NSL_ADefType)default_type, default_value,
		       &element->elsum, element->name))) {
      return 0;
    };

    return (AttributeDefinition)FindAttrSpec(element->elsum,
					     doctype,
					     name);
#else
    AttributeDefinition a;

    if(!(a= Malloc(sizeof(*a))) || !(name = Strndup(name, namelen)))
	return 0;

    a->name = name;
    a->namelen = namelen;
    a->type = type;
    a->allowed_values = allowed_values;
    a->default_type = default_type;
    a->default_value = default_value;
    a->next = element->attributes;
    element->attributes = a;

    return a;
#endif
}

AttributeDefinition FindAttributeN(ElementDefinition element,
				   const Char *name, int namelen)
{
#ifdef FOR_LT
    if(!element->doctype)
	return 0;

    if(!(name = AttrUniqueName(element->doctype, name, namelen)))
	return 0;
    return (AttributeDefinition)FindAttrSpec(element->elsum,
					     element->doctype,
					     name);
#else
    AttributeDefinition a;

    for(a = element->attributes; a; a = a->next)
	if(namelen == a->namelen &&
	   memcmp(name, a->name, namelen * sizeof(Char)) == 0)
	    return a;

    return 0;
#endif
}

AttributeDefinition NextAttributeDefinition(ElementDefinition element,
					    AttributeDefinition previous)
{
#ifdef FOR_LT
    return 0;			/* not implemented */
#else
    if(previous)
	return previous->next;
    else
	return element->attributes;
#endif
}

/* Free an attribute definition */

void FreeAttributeDefinition(AttributeDefinition a)
{
#ifndef FOR_LT
    /* We don't keep anything in the NSL case */

    if(!a)
	return;

    /* Free the name */

    Free((void *)a->name);

    /* Free the allowed values - we rely on them being allocated as in
       xmlparser.c: the Char * pointers point into one single block. */

    if(a->allowed_values)
	Free(a->allowed_values[0]);
    Free(a->allowed_values);

    /* Free the default value */

    Free((void *)a->default_value);

    /* Free the attribute definition itself */

    Free(a);
#endif
}

/* Notations */

/* 
 * Define a new notation.  The name is copied, the system and public ids are
 * not, but they are freed when the notation definition is freed!
 */

NotationDefinition DefineNotationN(Dtd dtd, const Char *name, int namelen, 
				  const char8 *publicid, const char8 *systemid)
{
    NotationDefinition n;

    if(!(n = Malloc(sizeof(*n))) || !(name = Strndup(name, namelen)))
	return 0;

    n->name = name;
    n->tentative = 1;
    n->systemid = systemid;
    n->publicid = publicid;
    n->next = dtd->notations;
    dtd->notations = n;

    return n;
}

NotationDefinition TentativelyDefineNotationN(Dtd dtd,
					      const Char *name, int namelen)
{
    NotationDefinition n;

    if(!(n = Malloc(sizeof(*n))) || !(name = Strndup(name, namelen)))
	return 0;

    n->name = name;
    n->tentative = 1;
    n->systemid = 0;
    n->publicid = 0;
    n->next = dtd->notations;
    dtd->notations = n;

    return n;
}

NotationDefinition RedefineNotation(NotationDefinition n,
				  const char8 *publicid, const char8 *systemid)
{
    n->tentative = 0;
    n->systemid = systemid;
    n->publicid = publicid;

    return n;
}

NotationDefinition FindNotationN(Dtd dtd, const Char *name, int namelen)
{
    NotationDefinition n;

    for(n = dtd->notations; n; n = n->next)
	if(Strncmp(name, n->name, namelen) == 0 && n->name[namelen] == 0)
	    return n;

    return 0;
}

void FreeNotationDefinition(NotationDefinition n)
{
    if(!n)
	return;

    /* Free the name */

    Free((void *)n->name);

    /* Free the ids */

    Free((void *)n->systemid);
    Free((void *)n->publicid);

    /* Free the notation definition itself */

    Free(n);
}
