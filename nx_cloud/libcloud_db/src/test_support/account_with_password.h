#pragma once

#include <cdb/account_data.h>

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
