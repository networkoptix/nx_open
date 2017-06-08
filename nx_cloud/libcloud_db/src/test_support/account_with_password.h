#pragma once

#include <nx/cloud/cdb/api/account_data.h>

namespace nx {
namespace cdb {

class AccountWithPassword:
    public api::AccountData
{
public:
    std::string password;
};

} // namespace cdb
} // namespace nx
