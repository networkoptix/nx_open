////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "resourcecontainer.h"


namespace stree
{
    ResourceContainer::ResourceContainer()
    :
        m_first( false )
    {
    }

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

    void ResourceContainer::goToBeginning() 
    {
        m_curIter = m_mediaStreamPameters.begin();
        m_first = true;
    }

    bool ResourceContainer::next()
    {
        if( m_curIter == m_mediaStreamPameters.end() )
            return false;
        if( m_first )
            m_first = false;
        else
            ++m_curIter;
        return m_curIter != m_mediaStreamPameters.end();
    }

    int ResourceContainer::resID() const
    {
        return m_curIter->first;
    }

    QVariant ResourceContainer::value() const
    {
        return m_curIter->second;
    }

    QString ResourceContainer::toString( const stree::ResourceNameSet& rns ) const
    {
        QString str;
        for( auto resPair: m_mediaStreamPameters )
        {
            if( !str.isEmpty() )
                str += QLatin1String(", ");
            str += lit("%1:%2").arg(rns.findResourceByID(resPair.first).name).arg(resPair.second.toString());
        }
        return str;
    }


    SingleResourceContainer::SingleResourceContainer()
    :
        m_resID( 0 )
    {
    }

    SingleResourceContainer::SingleResourceContainer(
        int resID,
        QVariant value )
    :
        m_resID( resID ),
        m_value( value )
    {
    }

    bool SingleResourceContainer::get( int resID, QVariant* const value ) const
    {
        if( resID != m_resID )
            return false;
        *value = m_value;
        return true;
    }

    void SingleResourceContainer::put( int resID, const QVariant& value )
    {
        m_resID = resID;
        m_value = value;
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
