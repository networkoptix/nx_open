/**********************************************************
* Aug 26, 2015
* a.kolesnikov
***********************************************************/

#include "system_data.h"

#include <utils/common/model_functions.h>

#include "cdb_ns.h"


namespace nx {
namespace cdb {
namespace data {

namespace SystemAccessRole
{
    QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS( Value,
        (SystemAccessRole::none, "none")
        (SystemAccessRole::owner, "owner")
        (SystemAccessRole::maintenance, "maintenance")
        (SystemAccessRole::viewer, "viewer")
        (SystemAccessRole::editor, "editor")
        (SystemAccessRole::editorWithSharing, "editorWithSharing")
    )
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS( SystemStatus,
    (ssInvalid, "invalid")
    (ssNotActivated, "notActivated")
    (ssActivated, "activated")
)


////////////////////////////////////////////////////////////
//// class SystemRegistrationData
////////////////////////////////////////////////////////////

SystemRegistrationData::SystemRegistrationData(SystemRegistrationData&& right)
:
    name(std::move(right.name))
{
}

SystemRegistrationData::SystemRegistrationData(const SystemRegistrationData& right)
:
    name(right.name)
{
}

bool SystemRegistrationData::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    return false;
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemRegistrationData* const systemData )
{
    if( !urlQuery.hasQueryItem("name") )
        return false;
    systemData->name = urlQuery.queryItemValue("name").toStdString();
    return true;
}


////////////////////////////////////////////////////////////
//// class SystemRegistrationDataWithAccountID
////////////////////////////////////////////////////////////

SystemRegistrationDataWithAccountID::SystemRegistrationDataWithAccountID(
    SystemRegistrationDataWithAccountID&& right)
:
    SystemRegistrationData(std::move(right)),
    accountID(std::move(right.accountID))
{
}

SystemRegistrationDataWithAccountID::SystemRegistrationDataWithAccountID(
    const SystemRegistrationDataWithAccountID& right)
:
    SystemRegistrationData(right),
    accountID(right.accountID)
{
}

////////////////////////////////////////////////////////////
//// class SystemData
////////////////////////////////////////////////////////////

SystemData::SystemData(SystemData&& right)
:
    id(std::move(right.id)),
    name(std::move(right.name)),
    authKey(std::move(right.authKey)),
    ownerAccountID(std::move(right.ownerAccountID)),
    status(std::move(right.status)),
    cloudConnectionSubscriptionStatus(std::move(right.cloudConnectionSubscriptionStatus))
{
}

SystemData::SystemData(const SystemData& right)
:
    id(right.id),
    name(right.name),
    authKey(right.authKey),
    ownerAccountID(right.ownerAccountID),
    status(right.status),
    cloudConnectionSubscriptionStatus(right.cloudConnectionSubscriptionStatus)
{
}

bool SystemData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case param::accountID:
            *value = QVariant::fromValue(ownerAccountID);
            return true;
        case param::systemID:
            *value = QVariant::fromValue(id);
            return true;
    }

    //TODO #ak
    return false;
}


////////////////////////////////////////////////////////////
//// class SystemDataList
////////////////////////////////////////////////////////////

SystemDataList::SystemDataList(const SystemDataList& right)
:
    systems(right.systems)
{
}

SystemDataList::SystemDataList(SystemDataList&& right)
:
    systems(std::move(right.systems))
{
}


////////////////////////////////////////////////////////////
//// class SystemSharing
////////////////////////////////////////////////////////////

SystemSharing::SystemSharing(const SystemSharing& right)
:
    accountID(right.accountID),
    systemID(right.systemID),
    accessRole(right.accessRole)
{
}

SystemSharing::SystemSharing(SystemSharing&& right)
:
    accountID(std::move(right.accountID)),
    systemID(std::move(right.systemID)),
    accessRole(std::move(right.accessRole))
{
}

bool SystemSharing::getAsVariant( int /*resID*/, QVariant* const /*value*/ ) const
{
    //TODO #ak
    return false;
}

bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing )
{
    if (!urlQuery.hasQueryItem("systemID") || !urlQuery.hasQueryItem("accountID"))
        return false;

    systemSharing->systemID = QnUuid::fromStringSafe(urlQuery.queryItemValue("systemID"));
    systemSharing->accountID = QnUuid::fromStringSafe(urlQuery.queryItemValue("accountID"));
    bool success = false;
    systemSharing->accessRole = QnLexical::deserialized<SystemAccessRole::Value>(
        urlQuery.queryItemValue(lit("accessRole")), SystemAccessRole::none, &success );
    return success;
}


////////////////////////////////////////////////////////////
//// class SystemID
////////////////////////////////////////////////////////////

SystemID::SystemID(const SystemID& right)
:
    id(right.id)
{
}

SystemID::SystemID(SystemID&& right)
:
    id(std::move(right.id))
{
}

bool SystemID::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case param::systemID:
            *value = QVariant::fromValue(id);
            return true;
        default:
            return false;
    }
}

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID)
{
    if (!urlQuery.hasQueryItem("id"))
        return false;
    systemID->id = urlQuery.queryItemValue("id");
    return true;
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json)(sql_record),
    _Fields )

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json),
    _Fields )

}   //data
}   //cdb
}   //nx
