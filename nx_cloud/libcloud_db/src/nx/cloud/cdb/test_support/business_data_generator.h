#pragma once

#include <string>

#include "account_with_password.h"
#include "../data/account_data.h"
#include "../data/system_data.h"

namespace nx {
namespace cdb {
namespace test {

class BusinessDataGenerator
{
public:
    static std::string generateRandomEmailAddress();

    static AccountWithPassword generateRandomAccount();

    static data::SystemData generateRandomSystem(const api::AccountData& account);

    static std::string generateRandomSystemId();

    static api::SystemSharingEx generateRandomSharing(
        const api::AccountData& account,
        const std::string& systemId);
};

} // namespace test
} // namespace cdb
} // namespace nx
