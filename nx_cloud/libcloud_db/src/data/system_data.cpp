/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <utils/common/model_functions.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {
namespace data {


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

bool SystemRegistrationData::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    return false;
}


////////////////////////////////////////////////////////////
//// class SystemData
////////////////////////////////////////////////////////////

bool SystemData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case attr::systemID:
            *value = QVariant::fromValue(QString::fromStdString(id));
            return true;
        default:
            return false;
    }
}


////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

bool SystemSharing::getAsVariant( int resID, QVariant* const value ) const
{
    switch (resID)
    {
        case attr::accountEmail:
            *value = QString::fromStdString(accountEmail);
            return true;
        case attr::systemID:
            *value = QVariant::fromValue(QString::fromStdString(systemID));
            return true;
        case attr::systemAccessRole:
            *value = QVariant(QnLexical::serialized(accessRole));
            return true;
        default:
            return false;
    }
}


////////////////////////////////////////////////////////////
//// class SystemID
////////////////////////////////////////////////////////////

bool SystemID::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::systemID:
            *value = QVariant::fromValue(QString::fromStdString(systemID));
            return true;
        default:
            return false;
    }
}

}   //data
}   //cdb
}   //nx
