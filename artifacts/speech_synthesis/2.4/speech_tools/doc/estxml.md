XML support {#estxml}
================

There are three levels of support for XML with EST.

  - *Loading as an Utterance*: A built in XML parser allows text marked up 
    according to an XML DTD to be read into an EST_Utterance (see  \ref xmltoutterance).

  - *XML_Parser_Class*: A C++ class XML_Parser_Class which makes it 
    relatively simple to write specialised XML processing code.
  - *RXP*: The RXP XML parser is included and can be used 
    directly (\ref rxpparser). 

# Reading XML Text As An EST_Utterance {#xmltoutterance}

In order to read XML marked up text, the EST code must be
told how the XML markup should relate to the utterance
structure. This is done by annotating the DTD using which the
text is processed. 

There are two possible ways to anotate the DTD. Either a new
DTD can be created with the anotations added, or the
anotations can be included in the XML file.

**A new DTD**

To write a new DTD based on an existing one, you should include
the existing one as follows:
@code{.xml}
  <!-- Extended FooBar DTD for speech tools -->

  <!-- Include original FooBar DTD -->
  <!ENTITY % OldFooBarDTD PUBLIC "//Foo//DTD Bar"
			      "http://www.foo.org/dtds/org.dtd">
  %OldFooBarDTD;

  <!-- Your extensions, for instance... -->

  <!-- syn-node elements are nodes in the Syntax relation  -->
  <!ATTLIST syn-node relationNode CDATA #FIXED "Syntax" >
@endcode

**In the XML file**

Extensions to the DTD can be included in the 
`!DOCTYPE` declaration in the marked up
text. For instance:
@code{.xml}
  <?xml version='1.0'?>
  <!DOCTYPE utterance PUBLIC "//Foo//DTD Bar"
			  "http://www.foo.org/dtds/org.dtd"
[
<!-- Item elements are nodes in the Syntax relation  -->
<!ATTLIST item relationNode CDATA #FIXED "Syntax" >
]>

  <utterance>
  <!-- Actual markup starts here -->
@endcode

## Summary of DTD Anotations

The following attributes may be added to elements in your
DTD to describe it's relation to EST_Utterance structures.

  - *estUttFeats*: 	The value should be a comma separated list of
    attributes which should be set as features on the
    utterance. Each attribute can be either a simple
    identifier, or two identifiers separated by a colon `:`.
    
	  A value `foo:bar` causes the value of
	  the `foo` attribute of the element to be
	  set as the value of the Utterance feature `bar`.

	  A simple identifier `foo` causes the
	  `foo` attribute of the element to be
	  set as the value of the Utterance feature
	  `X_foo` where `X` is the
	  name of the element.

  - *estRelationFeat*: The value should be a comma separated list of
	  attributes which should be set as features on the
	  relation related to this element. It's format and
  	meaning is the same as for `estUttFeats`. 

  - *estRelationElementAttr*: Indicates that this element defines a relation. All
	elements inside this one will be made nodes in the
	relation, unless they are explicitly marked to be
	ignored by *estRelationIgnore*. The
	value of the *estRelationElementAttr*
	attribute is the name of an attribute which gives the
	name of the relation. 

  - *estRelationTypeAttr*: When an element has a
	*estRelationElementAttr* tag to indicate it's
	content defines a relaion, it may also have the
	*estRelationTypeAttr* tag. This gives
	the name of an attribute which gives the type of
	relation. Currently only a type of `list' or `linear'
	gives a lienar relation, anything else gives a tree.

  - *estRelationIgnore*: If this is set to any value on an element which would
	otherwise be interpreted as an EST_Item in the current
	relation, the element is passed over. The contents
	will be processed as if they had been directly inside
	this element's parent.

  - *estRelationNode*: When placed on an element, indicates that this element
	is to be interpreted as an item in the relation named
	in the value of the attribute.

  - *estExpansion*: The value of this attribute defines how ranges in
	*href* attributes are expanded for
	this element. If the value is `replace`
	the nodes created during expansion are placed at the
	same level in the hierachy as the original element. If
	the value is `embed` they are created as
	children of a new node.

  - *estContentFeature*: The value of this attribute is the featre which is set
	to the contents of the current element.

# The XML_Parser_Class C++ Class {#xmlparserclass}

The C++ class XML_Parser_Class
(declared in \ref rxp/XML_Parser.h) defines an
abstract interface to the XML parsing process. By
breating a sub-class of XML_Parser_Class you can create code to
read XML marked up text quite simply.

## Some Definitions {#xmlparserclassdefinitions}

  - An XML parser is an object which can
    analyse a piece of text marked up according to an XML
    doctype and perform actions based on the markup. One
    XML parser deals with one text.

  - An XML parser is represented by an instance of the
    class XML_Parser.
    
  - An XML parser class is an object from which
    XML parses can be created. It defines the behaviour of
    the parsers when they process their assigned text, and
    also a mapping from XML entity IDs to places to look
    for them.

  - An XML parser class is represented by an instance of
    XML_Parser_Class or a subclass of XML_Parser_Class.

## Creating An XML Processing Procedure {#xmlcreatingxmlproc}

In order to create a procedure which will process XML
marked up text in the manner of your choice you need to do 4
things. Simple examples can be found in \ref testsuite/xml_example.cc and
\ref main/xml_parser_main.cc.


### Create a Sub-Class of XML_Parser_Class

Not written

### Create a Structure Holding the State of the Parse

Not written

### Decide How Entity IDs Should Be Converted To Filenames

Not written

### Write A Procedure To Start The Parser

Not written

## The XML_Parser_Class in Detail

Not written

 - XMLParser

# The RXP XML Parser {#rxpparser}

Included in the EST library is a version of the *RXP XML parser*.
This version is limited to 8-bit characters for consistency with the rest of
EST. For more details, see the *RXP* documentation. 

Insert reference to *RXP* documentation here.


