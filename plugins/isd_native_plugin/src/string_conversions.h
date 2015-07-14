/**********************************************************
* Jul 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_STRING_CONVERSIONS_H
#define NX_STRING_CONVERSIONS_H

#include <string>


template<class T> T convert( const std::string& str );

template<> int convert<int>( const std::string& str );

#endif  //NX_STRING_CONVERSIONS_H
