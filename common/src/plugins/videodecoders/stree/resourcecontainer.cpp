////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "resourcecontainer.h"


namespace stree
{
    bool ResourceContainer::get( int resID, QVariant* value ) const
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
}
