/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "wildcard_match.h"


template<class CharType>
bool wildcardMatch( const CharType* mask, const CharType* str )
{
    while( *str && *mask )
    {
        switch( *mask )
        {
            case (CharType)'*':
            {
                if( !*(mask+1) )
                    return true;    //we have * on mask end
                for( const CharType*
                    str1 = str;
                    *str1;
                    ++str1 )
                {
                    if( wildcardMatch( mask+1, str1 ) )
                        return true;
                }
                return false;   //match did not succeed
            }
            case (CharType)'?':
                if( *str == (CharType)'.' )
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
        if( *mask != (CharType)'*' )
            return false;
        ++mask;
    }

    if( *str )
        return false;

    return true;
}


bool wildcardMatch( const char* mask, const char* str )
{
    return wildcardMatch<>( mask, str );
}

bool wildcardMatch( const wchar_t* mask, const wchar_t* str )
{
    return wildcardMatch<>( mask, str );
}
