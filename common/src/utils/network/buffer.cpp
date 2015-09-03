/**********************************************************
* Aug 27, 2015
* a.kolesnikov
***********************************************************/

#include "buffer.h"

#include <cstring>


namespace nx
{
    bool operator==( const std::string& left, const nx::String& right )
    {
        if( left.size() != right.size() )
            return false;
        return memcmp( left.data(), right.constData(), left.size() ) == 0;
    }

    bool operator==( const nx::String& left, const std::string& right )
    {
        return right == left;
    }

    bool operator!=( const std::string& left, const nx::String& right )
    {
        return !(left == right);
    }

    bool operator!=( const nx::String& left, const std::string& right )
    {
        return right != left;
    }
}
