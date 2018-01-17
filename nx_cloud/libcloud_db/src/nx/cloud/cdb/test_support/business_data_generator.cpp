#include "business_data_generator.h"

#include <sstream>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <utils/common/app_info.h>
#include <utils/common/id.h>

namespace nx {
namespace cdb {
namespace test {

std::string BusinessDataGenerator::generateRandomEmailAddress()
{
    std::ostringstream ss;
    ss << "test_" << nx::utils::random::number<unsigned int>() << "@networkoptix.com";
    return ss.str();
}

AccountWithPassword BusinessDataGenerator::generateRandomAccount()
{
    AccountWithPassword account;
    account.id = QnUuid::createUuid().toSimpleString().toStdString();
    account.email = generateRandomEmailAddress();
    account.fullName = account.email + "_fullname";
    account.customization = QnAppInfo::customizationName().toStdString();
    account.statusCode = api::AccountStatus::activated;

    account.password = QnUuid::createUuid().toSimpleString().toStdString();
    account.passwordHa1 = nx::network::http::calcHa1(
        QUrl::fromPercentEncoding(QByteArray(account.email.c_str())).toStdString().c_str(),
        nx::network::AppInfo::realm().toStdString().c_str(),
        account.password.c_str()).constData();
    account.passwordHa1Sha256 = nx::network::http::calcHa1(
        QUrl::fromPercentEncoding(QByteArray(account.email.c_str())).toStdString().c_str(),
        nx::network::AppInfo::realm().toStdString().c_str(),
        account.password.c_str(),
        "SHA-256").constData();

    return account;
}

data::SystemData BusinessDataGenerator::generateRandomSystem(const api::AccountData& account)
{
    using namespace std::chrono;

    data::SystemData system;

    system.id = generateRandomSystemId();
    system.name = system.id;
    system.customization = QnAppInfo::customizationName().toStdString();
    //system.opaque = newSystem.opaque;
    system.authKey = QnUuid::createUuid().toSimpleString().toStdString();
    system.ownerAccountEmail = account.email;
    system.status = api::SystemStatus::activated;
    system.expirationTimeUtc =
        duration_cast<seconds>(nx::utils::timeSinceEpoch() + hours(1)).count();

    return system;
}

std::string BusinessDataGenerator::generateRandomSystemId()
{
    return QnUuid::createUuid().toSimpleByteArray().toStdString();
}

api::SystemSharingEx BusinessDataGenerator::generateRandomSharing(
    const api::AccountData& account,
    const std::string& systemId)
{
    api::SystemSharingEx sharing;
    sharing.accessRole = api::SystemAccessRole::cloudAdmin;
    sharing.accountEmail = account.email;
    sharing.accountFullName = account.fullName;
    sharing.accountId = account.id;
    sharing.isEnabled = true;
    sharing.lastLoginTime = nx::utils::utcTime();
    sharing.systemId = systemId;
    sharing.vmsUserId = guidFromArbitraryData(
        sharing.accountEmail).toSimpleString().toStdString();

    return sharing;
}

} // namespace test
} // namespace cdb
} // namespace nx
