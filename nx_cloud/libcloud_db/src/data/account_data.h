/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CDB_ACCOUNT_DATA_H
#define CDB_ACCOUNT_DATA_H

#include <string>
#include <stdint.h>

#include <plugins/videodecoder/stree/resourcecontainer.h>


namespace cdb {
namespace data {

//TODO #ak bring in fusion

class AccountData
:
    public stree::AbstractResourceReader
{
public:
    std::string email;
    //!HA1 (see rfc2617) digest of user's password. Realm is usually NetworkOptix
    std::basic_string<uint8_t> passwordHA1;
    std::basic_string<uint8_t> emailVerificationCode;
    //TODO

    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class EmailVerificationCode
{
public:
    std::basic_string<uint8_t> code;
};

}   //data
}   //cdb

#endif  //CDB_ACCOUNT_DATA_H
