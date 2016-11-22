/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <nx/fusion/model_functions.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {

namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemSharingEx),
    (sql_record),
    _Fields);

} // namespace api

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
SystemData::SystemData()
:
    expirationTimeUtc(0)
{
}

bool SystemData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case attr::systemId:
            *value = QString::fromStdString(id);
            return true;
        case attr::systemStatus:
            *value = static_cast<int>(status);
            return true;
        default:
            return false;
    }
}


////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

bool SystemSharing::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::accountEmail:
            *value = QString::fromStdString(accountEmail);
            return true;
        case attr::systemId:
            *value = QString::fromStdString(systemId);
            return true;
        case attr::systemAccessRole:
            *value = QVariant(QnLexical::serialized(accessRole));
            return true;
        default:
            return false;
    }
}

bool SystemSharingList::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    return false;
}

bool loadFromUrlQuery(
    const QUrlQuery& /*urlQuery*/,
    SystemSharingList* const /*systemSharing*/)
{
    return false;
}


////////////////////////////////////////////////////////////
//// class SystemId
////////////////////////////////////////////////////////////

bool SystemId::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::systemId:
            *value = QString::fromStdString(systemId);
            return true;
        default:
            return false;
    }
}


////////////////////////////////////////////////////////////
//// class SystemAttributesUpdate
////////////////////////////////////////////////////////////

bool SystemAttributesUpdate::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::systemId:
            *value = QString::fromStdString(systemId);
            return true;
        default:
            return false;
    }
}

//-------------------------------------------------------------------------------------------------
// class UserSessionDescriptor

bool UserSessionDescriptor::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::systemId:
            if (!systemId)
                return false;
            *value = QString::fromStdString(*systemId);
            return true;

        case attr::accountEmail:
            if (!accountEmail)
                return false;
            *value = QString::fromStdString(*accountEmail);
            return true;

        default:
            return false;
    }
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemId),
    (sql_record),
    _Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemData),
    (sql_record),
    _FieldsEx);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemSharing),
    (sql_record),
    _FieldsEx);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemSharingList),
    (json),
    _Fields);

}   //data
}   //cdb
}   //nx
