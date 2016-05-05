/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CDB_ACCOUNT_DATA_H
#define CDB_ACCOUNT_DATA_H

#include <QtCore/QUrlQuery>

#include <cstdint>
#include <string>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <utils/fusion/fusion_fwd.h>

#include <cloud_db_client/src/data/account_data.h>


namespace nx {
namespace cdb {
namespace data {

class AccountData
:
    public api::AccountData,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class AccountConfirmationCode
:
    public api::AccountConfirmationCode,
    public stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class AccountUpdateData
:
    public api::AccountUpdateData,
    public stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class AccountUpdateDataWithEmail
:
    public AccountUpdateData
{
public:
    std::string email;

    AccountUpdateDataWithEmail(AccountUpdateData&& rhs);
};

class AccountEmail
:
    public api::AccountEmail,
    public stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class AccessRestrictions
{
public:
    std::vector<std::string> requestsAllowed;

    std::string toString() const;
    bool parse(const std::string& str);

    bool authorize(const stree::AbstractResourceReader& requestAttributes) const;
};

class TemporaryAccountPassword
{
public:
    std::string accountEmail;
    std::string password;
    std::string passwordHa1;
    std::string realm;
    std::uint32_t expirationTimestampUtc;
    int maxUseCount;
    int useCount;
    //!this password has been sent to user's email address
    bool isEmailCode;
    AccessRestrictions accessRights;

    TemporaryAccountPassword();
};

#define TemporaryAccountPassword_Fields (accountEmail)(passwordHa1)(realm) \
    (expirationTimestampUtc)(maxUseCount)(useCount)(isEmailCode)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountPassword),
    (sql_record))

//#define AccountUpdateDataWithEmail_Fields (passwordHa1)(fullName)(customization)(email)
//
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
//    (AccountUpdateDataWithEmail),
//    (sql_record))

}   //data
}   //cdb
}   //nx

#endif  //CDB_ACCOUNT_DATA_H
