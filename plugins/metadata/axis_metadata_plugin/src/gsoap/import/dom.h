/*

        dom.h

        gSOAP DOM API v5
         
        Use #import "dom.h" in gSOAP header files to use DOM-based xsd__anyType
        for XML element and xsd__anyAttribute for XML attributes.
        
        This import is automatic with wsdl2h option -d.

        Compile with dom.c[pp] or link with libgsoapssl.a (for C) or
        libgsoapssl++.a (for C++)

        See gsoap/doc/dom/html/index.html for the new DOM API v5 documentation
        Also located in /gsoap/samples/dom/README.md

gSOAP XML Web services tools
Copyright (C) 2000-2016, Robert van Engelen, Genivia, Inc. All Rights Reserved.
This part of the software is released under ONE of the following licenses:
GPL, or the gSOAP public license, or Genivia's license for commercial use.
--------------------------------------------------------------------------------
gSOAP public license.

The contents of this file are subject to the gSOAP Public License Version 1.3
(the "License"); you may not use this file except in compliance with the
License. You may obtain a copy of the License at
http://www.cs.fsu.edu/~engelen/soaplicense.html
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the License.

The Initial Developer of the Original Code is Robert A. van Engelen.
Copyright (C) 2000-2016, Robert van Engelen, Genivia Inc., All Rights Reserved.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org

This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

// The custom serializer for DOM element nodes is represented by xsd__anyType.
extern typedef struct soap_dom_element xsd__anyType;

// The custom serializer for DOM attribute nodes is represented by @xsd__anyAttribute.
extern typedef struct soap_dom_attribute xsd__anyAttribute;
