#pragma once

#include <nx/cloud/cdb/api/account_data.h>

namespace nx::cloud::db {

class AccountWithPassword:
    public api::AccountData
{
public:
    std::string password;
};

} // namespace nx::cloud::db
