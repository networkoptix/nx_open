/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef WILDCARDMATCH_H
#define WILDCARDMATCH_H

#include <cstdlib>


//!Matches \a str by wildcard expression \a mask
/*!
    \param mask Mask. Can contain special symbols ? (any symbol except .) and * (any quantity of any symbols)
    \param str Text to match
    \return a true if \a str satisfies \a mask
*/
bool wildcardMatch( const char* mask, const char* str );
bool wildcardMatch( const wchar_t* mask, const wchar_t* str );

#endif  //WILDCARDMATCH_H
