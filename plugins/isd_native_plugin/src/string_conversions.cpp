/**********************************************************
* Jul 14, 2015
* a.kolesnikov
***********************************************************/

#include "string_conversions.h"


template<> int convert<int>( const std::string& str )
{
    return std::stoi( str );
}
