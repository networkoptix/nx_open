#include "business_data_generator.h"

#include <sstream>

#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <utils/common/app_info.h>

namespace nx {
namespace cdb {
namespace test {

std::string BusinessDataGenerator::generateRandomEmailAddress()
{
    std::ostringstream ss;
    ss << "test_" << nx::utils::random::number<unsigned int>() << "@networkoptix.com";
    return ss.str();
}

api::AccountData BusinessDataGenerator::generateRandomAccount()
{
    api::AccountData account;
    account.id = QnUuid::createUuid().toSimpleString().toStdString();
    account.email = generateRandomEmailAddress();
    account.fullName = account.email + "_fullname";
    account.customization = QnAppInfo::customizationName().toStdString();
    account.statusCode = api::AccountStatus::activated;

    return account;
}

data::SystemData BusinessDataGenerator::generateRandomSystem(const api::AccountData& account)
{
    using namespace std::chrono;

    data::SystemData system;

    system.id = QnUuid::createUuid().toSimpleByteArray().toStdString();
    system.name = system.id;
    system.customization = QnAppInfo::customizationName().toStdString();
    //system.opaque = newSystem.opaque;
    system.authKey = QnUuid::createUuid().toSimpleString().toStdString();
    system.ownerAccountEmail = account.email;
    system.status = api::SystemStatus::ssActivated;
    system.expirationTimeUtc =
        duration_cast<seconds>(nx::utils::timeSinceEpoch() + hours(1)).count();

    return system;
}

} // namespace test
} // namespace cdb
} // namespace nx
