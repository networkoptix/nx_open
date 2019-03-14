#pragma once

#include <string>

#include "account_with_password.h"
#include "../data/account_data.h"
#include "../data/system_data.h"

namespace nx::cloud::db {
namespace test {

class BusinessDataGenerator
{
public:
    static std::string generateRandomEmailAddress();

    /**
     * @param email If empty, then random email address is generated.
     */
    static AccountWithPassword generateRandomAccount(
        std::string email = std::string());

    static data::SystemData generateRandomSystem(const api::AccountData& account);

    static std::string generateRandomSystemId();

    static api::SystemSharingEx generateRandomSharing(
        const api::AccountData& account,
        const std::string& systemId);
};

} // namespace test
} // namespace nx::cloud::db
