////////////////////////////////////////////////////////////
// 2 oct 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "wildcard.h"


bool wildcardMatch( const char* mask, const char* str )
{
    while( *str && *mask )
    {
        switch( *mask )
        {
            case '*':
            {
                if( !*(mask+1) )
                    return true;    //we have * at the end
                for( const char*
                    str1 = str;
                    *str1;
                    ++str1 )
                {
                    if( wildcardMatch( mask+1, str1 ) )
                        return true;
                }
                return false;   //not validated
            }
            case '?':
                if( *str == '.' )
                    return false;
                break;

            default:
                if( *str != *mask )
                    return false;
                break;
        }

        ++str;
        ++mask;
    }

    while( *mask )
    {
        if( *mask != '*' )
            return false;
        ++mask;
    }

    if( *str )
        return false;

    return true;
}
