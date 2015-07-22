/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#ifndef ACCOUNT_DATA_H
#define ACCOUNT_DATA_H

#include <string>
#include <stdint.h>


class AccountData
{
public:
    std::string login;
    //!HA1 digest of user's password
    std::basic_string<uint8_t> passwordDigest;
    std::basic_string<uint8_t> emailVerificationCode;
    //TODO
};

#endif  //ACCOUNT_DATA_H
