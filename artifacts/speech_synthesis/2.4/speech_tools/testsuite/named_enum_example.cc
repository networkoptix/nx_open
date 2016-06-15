 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
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
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)            */
 /*                   Date: Tue Apr 29 1997                              */
 /************************************************************************/
 /*                                                                      */
 /* Example of the declaration and use fo the named enum type.           */
 /*                                                                      */
 /************************************************************************/

#include <cstdlib>
#include <iostream>
#include "EST_TNamedEnum.h"

#if defined(DATAC)
#    define __STRINGIZE(X) #X
#    define DATA __STRINGIZE(DATAC)
#endif

// the named enum class provides an easy way to associate strings with
// the values of and enumerated type, for instance for IO. It's type safe and
// as readable as seems possible in C++.

// A single enum element can have multiple names (eg synonyms or common typos)

// Named enums are defined in terms of the more general valued enums which
// associates enum elements with members of an arbitrary type.

// Both named enums and valued enums can have an additional piece of
// information (eg a struct containing creation functions or another
// representation) associated with each enum element.

//  --------- Declaration (eg in header or class) ------------

// the enumerated type used in C code.

typedef enum { c_red=1, c_blue=2, c_green=3, c_unknown=666} Colour;

// the mapping used to get names and so on
extern EST_TNamedEnum<Colour> ColourMap;

// ---------- Definition ---------------------------------------
// In a .C file somewhere we have to give the names in a table. 
// The table has to be given a name and is then used to initialise the mapping.

// The definition table ends with a repeat of the first entry and some
// value. This last pair gives the unknown enum value and unknown
// value for use when lookup fails

// For reasons of C++ brain death we have to declare this as a 
// ValuedEnumDefinition<X,const char *, NO_INFO> rather than a
// NamedEnum<X>::Definition or some other saner version

Start_TNamedEnum(Colour, ColourMap)
//  enum element         list of names
  { c_unknown,		{"kinda brownish"}},
  { c_red,		{"red", "scarlet"}},
  { c_blue,		{"blue", "navy", "sad"}},
  { c_unknown,		{NULL}} // looking up unknown names gives c_unknown
				// looking up unknown enum values gives NULL
End_TNamedEnum(Colour, ColourMap)

// Here is a different table for the same enum type. 
// Perhaps we want to accept input in Spanish, but not get Spanish and
// English names mixed up

Start_TNamedEnum(Colour, SpanishColourMap)
//  enum element         list of names
  { c_unknown,		{"no conocido"}},
  { c_red,		{"rojo", "escarlata", "sangre"}},
  { c_blue,		{"azul", "piscina", "mar", "cielo"}},
  { c_unknown,		{NULL}} 
End_TNamedEnum(Colour, SpanishColourMap)

// ------- Alternative including extra information ---------------

// Sometimes you may want to associate information with each element.
// The following variant associates three small integers with each
// colour, perhaps to enable them to be displayed. 

struct colour_info {
  int red, green, blue;
};

// a map including this extra information is declared as
extern EST_TNamedEnumI<Colour, colour_info> RGBColourMap;


// and here is how the values are defined.

Start_TNamedEnumI(Colour, colour_info,  RGBColourMap)
//  enum element  list of names             extra info (red, green, blue)
  { c_unknown,	{"kinda grey"},			{0x7f, 0x7f, 0x7f}},
  { c_red,	{"red", "scarlet"},		{0xff, 0, 0}},
  { c_blue,	{"blue", "navy", "sad"},	{0, 0, 0xff}},
  { c_unknown,	{NULL}}
End_TNamedEnumI(Colour, colour_info,  RGBColourMap)



// --------- Use -----------------------------------------------

int main(void)
{
  Colour c1 = c_red;
  Colour c2 = c_green;
  const char *n;

  // get the default name for colours.
  n = ColourMap.name(c1);
  cout << "c1 is " << (n?n:"[NULL]") << "\n";

  n = ColourMap.name(c2);
  cout << "c2 is " << (n?n:"[NULL]") << "\n";


  // look up some names to see what they correspond to
  const char *colours[] = { "red", "navy", "puce"};
  for(int i=0; i<3; i++)
    {
      // note since enum values are universal
      // we can get the colour by assuming English, get the
      // information from the other map and get the name for output in
      // spanish
      const char *nm= colours[i];
      Colour c = ColourMap.token(nm);
      colour_info &info = RGBColourMap.info(c);
      const char *spanish = SpanishColourMap.name(c);

      cout << nm << " is " << (int)c
	   << " = " << ColourMap.name(c) 
	   << " (" << (spanish?spanish:"[NULL]") << " in Spanish)"
	   << " = {" 
	   << info.red << ", "
	   << info.green << ", "
	   << info.blue
	   << "}\n";
    }

  // In the special case of EST_TNamedEnum (i.e. simple mappings from
  // enum to (const char *) with no extra information) we can save
  // mappings to files and read them back

  // There are two ways to save a mapping, we can have the file say how
  // names map to numeric values...

  if (ColourMap.save("tmp/colour.map") != write_ok)
    cout << "\n\nname map write failed\n";
  else
    {
      cout << "\n\ncolour name map\n";
      cout.flush();
      system("cat tmp/colour.map");
    }

  // Of course this can result in the file not being valid when the
  // enumerated type definition changes and so the assigned numbers change.

  // If there is a standard mapping defined and we are saving an
  // alternative, the standard one can provide names for the enumerated
  // values, meaning the file will be valid so long as the default
  // mapping is correct.

  // For instance we can assume someone maintains the English names
  // as the type is extended and save the Spanish names as a translation.
  
  if (SpanishColourMap.save("tmp/colour_spanish.map", ColourMap) != write_ok)
    cout << "\n\nname map write failed\n";
  else
    {
      cout << "\n\ncolour name map (spanish)\n";
      cout.flush();
      system("cat tmp/colour_spanish.map");
    }

  // There are two corresponding ways to read in a map.

  // If the map is defined in the file by numbers we load it like this...

  EST_TNamedEnum<Colour> LoadedColourMap(c_unknown);
  if (LoadedColourMap.load(DATA "/colours.map") !=format_ok)
    cout << "\n\nname map read failed\n";
  else
    {
      cout << "\n\nread in table\n";
      LoadedColourMap.save("tmp/tmp.map");
      cout.flush();
      system("cat tmp/tmp.map");
    }

  // If it's defined in the file using the names... 

  if (LoadedColourMap.load(DATA "/colours_translation.map", ColourMap) !=format_ok)
    cout << "\n\nname map read failed\n";
  else
    {
      cout << "\n\nread in table (translation)\n";

      LoadedColourMap.save("tmp/tmp.map");
      cout.flush();
      system("cat tmp/tmp.map");
    }

  exit(0);
}

// ----------- Template Brain Death --------------------

// Declaration of the template use for GCC
// Just which variants need to be declared is sometimes unpredictable,
// especially between versions of gcc.
// Best just compile and then find out which ones aren't there.

Declare_TNamedEnumI(Colour, colour_info)
Declare_TNamedEnum(Colour)

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TNamedEnum.cc"

Instantiate_TNamedEnumI(Colour, colour_info)
Instantiate_TNamedEnum(Colour)

#endif
