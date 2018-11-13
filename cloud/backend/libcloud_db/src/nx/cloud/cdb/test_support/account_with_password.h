#pragma once

#include <nx/cloud/cdb/api/account_data.h>

namespace nx::cdb {

class AccountWithPassword:
    public api::AccountData
{
public:
    std::string password;
};

} // namespace nx::cdb
