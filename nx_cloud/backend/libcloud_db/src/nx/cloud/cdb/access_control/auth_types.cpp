/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "auth_types.h"


namespace nx {
namespace cdb {


AuthorizationInfo::AuthorizationInfo()
{
}

AuthorizationInfo::AuthorizationInfo( nx::utils::stree::ResourceContainer&& rc )
:
    m_rc( std::move(rc) )
{
}

bool AuthorizationInfo::getAsVariant( int resID, QVariant* const value ) const
{
    if( m_rc.getAsVariant( resID, value ) )
        return true;

    //TODO #ak
    return false;
}

std::unique_ptr<nx::utils::stree::AbstractConstIterator> AuthorizationInfo::begin() const
{
    return m_rc.begin();
}

bool AuthorizationInfo::accessAllowedToOwnDataOnly() const
{
    //TODO #ak
    return false;
}


}   //cdb
}   //nx
