/**********************************************************
* Sep 3, 2015
* NetworkOptix
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ACCOUNT_DATA_H
#define NX_CDB_ACCOUNT_DATA_H

#include <cstdint>
#include <chrono>
#include <string>

#include <boost/optional.hpp>


namespace nx {
namespace cdb {
namespace api {

enum class AccountStatus
{
    invalid = 0,
    awaitingActivation = 1,
    activated = 2,
    blocked = 3
};

class AccountData
{
public:
    std::string id;
    //!User email. Used as unique user id
    std::string email;
    //!Hex representation of HA1 (see rfc2617) digest of user's password. Realm is usually VMS
    std::string passwordHa1;
    std::string fullName;
    std::string customization;
    AccountStatus statusCode;

    AccountData()
    :
        statusCode(AccountStatus::invalid)
    {
    }
};

class AccountConfirmationCode
{
public:
    std::string code;
};

class AccountUpdateData
{
public:
    boost::optional<std::string> passwordHa1;
    boost::optional<std::string> fullName;
    boost::optional<std::string> customization;
};


class AccountEmail
{
public:
    std::string email;
};


class TemporaryCredentialsParams
{
public:
    std::chrono::seconds expirationPeriod;
    /** if \a true, each request, authorized with these credentials, 
        increases credentials life time by \a prolongationPeriod.
    */
    bool autoProlongationEnabled;
    std::chrono::seconds prolongationPeriod;

    TemporaryCredentialsParams()
    :
        autoProlongationEnabled(false)
    {
    }
};

class TemporaryCredentials
{
public:
    std::string login;
    std::string password;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_ACCOUNT_DATA_H
