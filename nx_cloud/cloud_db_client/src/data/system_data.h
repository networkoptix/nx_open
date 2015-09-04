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
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/system_data.h>


namespace nx {
namespace cdb {
namespace api {

namespace SystemAccessRole
{
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Value), (lexical))
}

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SystemStatus)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((SystemStatus), (lexical))

#define SystemRegistrationData_Fields (name)

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemRegistrationData* const systemData );


//TODO #ak add corresponding parser/serializer to fusion and remove this function
//bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemData* const systemData );

#define SystemData_Fields (id)(name)(authKey)(ownerAccountID)(status)(cloudConnectionSubscriptionStatus)


class SystemDataList
{
public:
    std::vector<SystemData> systems;
};

#define SystemDataList_Fields (systems)


bool loadFromUrlQuery( const QUrlQuery& urlQuery, SystemSharing* const systemSharing );

#define SystemSharing_Fields (accountID)(systemID)(accessRole)



QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing),
    (json)(sql_record) )

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemDataList),
    (json))


}   //api
}   //cdb
}   //nx

#endif //CLOUD_DB_CL_SYSTEM_DATA_H
