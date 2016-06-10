/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_CL_SYSTEM_DATA_H
#define CLOUD_DB_CL_SYSTEM_DATA_H

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include <cdb/system_data.h>


namespace nx {
namespace cdb {
namespace api {

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemStatus)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemStatus), (lexical))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemHealth)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemHealth), (lexical))

#define SystemRegistrationData_Fields (name)(customization)

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData);
void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery);


//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(customization)(authKey)(ownerAccountEmail)(status)(cloudConnectionSubscriptionStatus)
#define SystemDataList_Fields (systems)

//!for requests passing just system id
class SystemID
{
public:
    std::string systemID;

    SystemID();
    SystemID(std::string systemIDStr);
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID);
void serializeToUrlQuery(const SystemID& data, QUrlQuery* const urlQuery);

#define SystemID_Fields (systemID)


////////////////////////////////////////////////////////////
//// system sharing data
////////////////////////////////////////////////////////////

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemAccessRole)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemAccessRole), (lexical))

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing);
void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery);

#define SystemSharing_Fields (accountEmail)(systemID)(accessRole)
#define SystemSharingList_Fields (sharing)

#define SystemSharingEx_Fields SystemSharing_Fields(fullName)
#define SystemSharingExList_Fields (sharing)

#define SystemAccessRoleData_Fields (accessRole)
#define SystemAccessRoleList_Fields (accessRoles)

#define SystemDataEx_Fields SystemData_Fields(ownerFullName)(accessRole)(sharingPermissions)(stateOfHealth)
#define SystemDataExList_Fields (systems)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json));

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemDataEx)(SystemDataList)(SystemDataExList)(SystemSharingList)(SystemSharingEx) \
        (SystemSharingExList)(SystemAccessRoleData)(SystemAccessRoleList),
    (json));

}   //api
}   //cdb
}   //nx

#endif //CLOUD_DB_CL_SYSTEM_DATA_H
