////////////////////////////////////////////////////////////
// 28 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "resourcenameset.h"


using namespace std;

namespace stree
{
    bool ResourceNameSet::registerResource( int id, const QString& name, QVariant::Type type )
    {
        pair<ResNameToIDDict::iterator, bool> p = m_resNameToID.insert( 
            make_pair( name, ResourceDescription( id, name, type ) ) );
        if( !p.second )
            return false;

        if( !m_resIDToName.insert( make_pair( id, p.first ) ).second )
        {
            m_resNameToID.erase( p.first );
            return false;
        }

        return true;
    }

    ResourceNameSet::ResourceDescription ResourceNameSet::findResourceByName( const QString& resName ) const
    {
        ResNameToIDDict::const_iterator it = m_resNameToID.find( resName );
        return it == m_resNameToID.end()
            ? ResourceDescription()
            : it->second;
    }

    ResourceNameSet::ResourceDescription ResourceNameSet::findResourceByID( int resID ) const
    {
        ResIDToNameDict::const_iterator it = m_resIDToName.find( resID );
        return it == m_resIDToName.end()
            ? ResourceDescription()
            : it->second->second;
    }

    void ResourceNameSet::removeResource( int resID )
    {
        ResIDToNameDict::iterator it = m_resIDToName.find( resID );
        if( it == m_resIDToName.end() )
            return;
        m_resNameToID.erase( it->second );
        m_resIDToName.erase( it );
    }
}
