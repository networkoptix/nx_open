/**********************************************************
* Sep 4, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ACCOUNT_DATA_H
#define NX_CDB_CL_ACCOUNT_DATA_H

#include <QtCore/QUrlQuery>

#include <stdint.h>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include <cdb/account_data.h>


namespace nx {
namespace cdb {
namespace api {

////////////////////////////////////////////////////////////
//// class AccountData
////////////////////////////////////////////////////////////

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AccountStatus)
//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountData* const accountData);
void serializeToUrlQuery(const AccountData&, QUrlQuery* const urlQuery);

#define AccountData_Fields (id)(email)(passwordHa1)(fullName)(customization)(statusCode)


////////////////////////////////////////////////////////////
//// class AccountConfirmationCode
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountConfirmationCode* const data);
void serializeToUrlQuery(const AccountConfirmationCode&, QUrlQuery* const urlQuery);

#define AccountConfirmationCode_Fields (code)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AccountData)(AccountConfirmationCode),
    (json)(sql_record) )


////////////////////////////////////////////////////////////
//// class AccountUpdateData
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountUpdateData* const data);
void serializeToUrlQuery(const AccountUpdateData&, QUrlQuery* const urlQuery);

void serialize(QnJsonContext*, const AccountUpdateData&, QJsonValue*);
bool deserialize(QnJsonContext*, const QJsonValue&, AccountUpdateData*);


////////////////////////////////////////////////////////////
//// class AccountEmail
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AccountEmail* const data);
void serializeToUrlQuery(const AccountEmail&, QUrlQuery* const urlQuery);

#define AccountEmail_Fields (email)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AccountEmail),
    (json))


////////////////////////////////////////////////////////////
//// class TemporaryCredentials
////////////////////////////////////////////////////////////

bool loadFromUrlQuery(const QUrlQuery& urlQuery, TemporaryCredentialsTimeouts* const data);
void serializeToUrlQuery(const TemporaryCredentialsTimeouts&, QUrlQuery* const urlQuery);

bool loadFromUrlQuery(const QUrlQuery& urlQuery, TemporaryCredentialsParams* const data);
void serializeToUrlQuery(const TemporaryCredentialsParams&, QUrlQuery* const urlQuery);

#define TemporaryCredentialsTimeouts_Fields (expirationPeriod)(autoProlongationEnabled)(prolongationPeriod)

#define TemporaryCredentialsParams_Fields (type)(timeouts)

#define TemporaryCredentials_Fields (login)(password)(timeouts)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (TemporaryCredentialsTimeouts)(TemporaryCredentialsParams)(TemporaryCredentials),
    (json))


}   //api
}   //cdb
}   //nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::api::AccountStatus), (lexical))

#endif  //NX_CDB_CL_ACCOUNT_DATA_H
