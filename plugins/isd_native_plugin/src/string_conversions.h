/**********************************************************
* Jul 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_STRING_CONVERSIONS_H
#define NX_STRING_CONVERSIONS_

#include <string>


template<class T> T convert( const std::string& str );

template<> int convert( const std::string& str )
{
    return std::stoi( str );
}

#endif  //NX_STRING_CONVERSIONS_
