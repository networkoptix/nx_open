////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "resourcecontainer.h"


namespace stree
{
    bool ResourceContainer::get( int resID, QVariant* const value ) const
    {
        std::map<int, QVariant>::const_iterator it = m_mediaStreamPameters.find( resID );
        if( it == m_mediaStreamPameters.end() )
            return false;

        *value = it->second;
        return true;
    }
    
    void ResourceContainer::put( int resID, const QVariant& value )
    {
        m_mediaStreamPameters[resID] = value;
    }


    MultiSourceResourceReader::MultiSourceResourceReader( const AbstractResourceReader& rc1, const AbstractResourceReader& rc2 )
    :
        m_rc1( rc1 ),
        m_rc2( rc2 )
    {
    }

    bool MultiSourceResourceReader::get( int resID, QVariant* const value ) const
    {
        return m_rc1.get( resID, value ) || m_rc2.get( resID, value );
    }
}
