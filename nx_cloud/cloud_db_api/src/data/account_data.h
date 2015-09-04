/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ACCOUNT_DATA_H
#define NX_CDB_CL_ACCOUNT_DATA_H

#include <QtCore/QUrlQuery>

#include <stdint.h>
#include <string>

#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cdb/account_data.h>


namespace nx {
namespace cdb {
namespace api {

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AccountStatus)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((AccountStatus), (lexical))

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountData* const accountData);
void serializeToUrlQuery(const AccountData&, QUrlQuery* const urlQuery);

#define AccountData_Fields (id)(login)(email)(passwordHa1)(fullName)(statusCode)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AccountData),
    (json)(sql_record) )


}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_CL_ACCOUNT_DATA_H
