/**********************************************************
* Sep 3, 2015
* NetworkOptix
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ACCOUNT_DATA_H
#define NX_CDB_ACCOUNT_DATA_H

#include <stdint.h>
#include <string>

#include <utils/common/uuid.h>


namespace nx {
namespace cdb {
namespace api {


enum AccountStatus
{
    asInvalid = 0,
    asAwaitingEmailConfirmation = 1,
    asActivated = 2,
    asBlocked = 3
};

class AccountData
{
public:
    QnUuid id;
    //!Unique user login
    std::string login;
    std::string email;
    //!Hex representation of HA1 (see rfc2617) digest of user's password. Realm is usually NetworkOptix
    std::string passwordHa1;
    std::string fullName;
    AccountStatus statusCode;

    AccountData()
    :
        statusCode( asInvalid )
    {
    }
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_ACCOUNT_DATA_H
