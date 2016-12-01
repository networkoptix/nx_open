#pragma once

#include <string>

#include "data/account_data.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace test {

class BusinessDataGenerator
{
public:
    static std::string generateRandomEmailAddress();
    static api::AccountData generateRandomAccount();
    static data::SystemData generateRandomSystem(const api::AccountData& account);
};

} // namespace test
} // namespace cdb
} // namespace nx
