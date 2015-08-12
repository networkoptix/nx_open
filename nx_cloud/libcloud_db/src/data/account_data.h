/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CDB_ACCOUNT_DATA_H
#define CDB_ACCOUNT_DATA_H

#include <QtCore/QUrlQuery>

#include <stdint.h>
#include <string>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/common/uuid.h>
#include <utils/fusion/fusion_fwd.h>


namespace nx {
namespace cdb {
namespace data {


enum AccountStatus
{
    asInvalid = 0,
    asAwaitingEmailConfirmation = 1,
    asActivated = 2,
    asBlocked = 3
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION( AccountStatus )
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (AccountStatus), (lexical) )

class AccountData
:
    public stree::AbstractResourceReader
{
public:
    QnUuid id;
    std::string email;
    //!Hex representgation of HA1 (see rfc2617) digest of user's password. Realm is usually NetworkOptix
    std::string passwordHa1;
    std::string fullName;
    AccountStatus statusCode;

    AccountData()
    :
        statusCode( asInvalid )
    {
    }

    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

//TODO #ak add corresponding parser/serializer to fusion and remove this function
void fromUrlQuery( const QUrlQuery& urlQuery, AccountData* const accountData );

#define AccountData_Fields (id)(email)(passwordHa1)(fullName)(statusCode)


class EmailVerificationCode
{
public:
    std::string code;
};

#define EmailVerificationCode_Fields (code)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AccountData)(EmailVerificationCode),
    (json)(sql_record) )


}   //data
}   //cdb
}   //nx

#endif  //CDB_ACCOUNT_DATA_H
