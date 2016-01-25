/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_CL_SYSTEM_DATA_H
#define CLOUD_DB_CL_SYSTEM_DATA_H

#include <QtCore/QUrlQuery>

#include <string>
#include <vector>

#include <utils/common/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/system_data.h>


namespace nx {
namespace cdb {
namespace api {

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemAccessRole)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemAccessRole), (lexical))

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemStatus)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemStatus), (lexical))

#define SystemRegistrationData_Fields (name)(customization)

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemRegistrationData* const systemData);
void serializeToUrlQuery(const SystemRegistrationData& data, QUrlQuery* const urlQuery);


//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(customization)(authKey)(ownerAccountEmail)(status)(cloudConnectionSubscriptionStatus)


#define SystemDataList_Fields (systems)


bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharing* const systemSharing);
void serializeToUrlQuery(const SystemSharing& data, QUrlQuery* const urlQuery);

#define SystemSharing_Fields (accountEmail)(systemID)(accessRole)

#define SystemSharingList_Fields (sharing)


//!for requests passing just system id
class SystemID
{
public:
    QnUuid systemID;

    SystemID();
    SystemID(std::string systemIDStr);
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemID* const systemID);
void serializeToUrlQuery(const SystemID& data, QUrlQuery* const urlQuery);

#define SystemID_Fields (systemID)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemID),
    (json)(sql_record));

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemDataList)(SystemSharingList),
    (json));


}   //api
}   //cdb
}   //nx

#endif //CLOUD_DB_CL_SYSTEM_DATA_H
