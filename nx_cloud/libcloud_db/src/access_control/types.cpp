/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "types.h"


namespace nx {
namespace cdb {


AuthorizationInfo::AuthorizationInfo()
{
}

AuthorizationInfo::AuthorizationInfo( stree::ResourceContainer&& rc )
{
}

bool AuthorizationInfo::getAsVariant( int resID, QVariant* const value ) const
{
    //TODO #ak
    return false;
}

bool AuthorizationInfo::accessAllowedToOwnDataOnly() const
{
    //TODO #ak
    return false;
}


}   //cdb
}   //nx
