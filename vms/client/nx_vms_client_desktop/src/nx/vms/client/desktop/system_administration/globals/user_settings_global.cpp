// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_global.h"

#include <QtQml/QQmlEngine>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/utils/password_information.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/text/time_strings.h>

namespace nx::vms::client::desktop {

namespace {

QObject* createUserSettingsGlobal(QQmlEngine*, QJSEngine*)
{
    return new UserSettingsGlobal();
}

} // namespace

const QString UserSettingsGlobal::kUsersSection = "U";
const QString UserSettingsGlobal::kGroupsSection = "G";
const QString UserSettingsGlobal::kBuiltInGroupsSection = "B";
const QString UserSettingsGlobal::kCustomGroupsSection = "C";
const QString UserSettingsGlobal::kLdapGroupsSection = "L";

UserSettingsGlobal::UserSettingsGlobal()
{
}

UserSettingsGlobal::~UserSettingsGlobal()
{
}

void UserSettingsGlobal::registerQmlTypes()
{
    qmlRegisterSingletonType<UserSettingsGlobal>(
        "nx.vms.client.desktop",
        1,
        0,
        "UserSettingsGlobal",
        &createUserSettingsGlobal);

    qRegisterMetaType<PasswordStrengthData>();
}

Q_INVOKABLE QUrl UserSettingsGlobal::accountManagementUrl() const
{
    core::CloudUrlHelper urlHelper(
        nx::vms::utils::SystemUri::ReferralSource::DesktopClient,
        nx::vms::utils::SystemUri::ReferralContext::SettingsDialog);

    return urlHelper.accountManagementUrl();
}

QString UserSettingsGlobal::validatePassword(const QString& password)
{
    auto validateFunction = extendedPasswordValidator();
    const auto result = validateFunction(password);
    return result.state != QValidator::Acceptable ? result.errorMessage : "";
}

PasswordStrengthData UserSettingsGlobal::passwordStrength(const QString& password)
{
    PasswordInformation info(password, nx::utils::passwordStrength);
    PasswordStrengthData result;

    result.text = info.text().toUpper();
    result.hint = info.hint();

    using namespace nx::vms::client::core;
    switch (info.acceptance())
    {
        case nx::utils::PasswordAcceptance::Good:
            result.background = core::colorTheme()->color("green_core");
            result.accepted = true;
            break;

        case nx::utils::PasswordAcceptance::Acceptable:
            result.background = core::colorTheme()->color("yellow_core");
            result.accepted = true;
            break;

        default:
            result.background = core::colorTheme()->color("red_core");
            result.accepted = false;
            break;
    }

    return result;
}

UserSettingsGlobal::UserType UserSettingsGlobal::getUserType(const QnUserResourcePtr& user)
{
    if (user->isCloud())
        return UserSettingsGlobal::CloudUser;

    if (user->isLdap())
        return UserSettingsGlobal::LdapUser;

    return UserSettingsGlobal::LocalUser;
}

QString UserSettingsGlobal::humanReadableSeconds(int seconds)
{
    static const std::vector<std::pair<QnTimeStrings::Suffix, int>> multipliers = {
        {QnTimeStrings::Suffix::Days, 60 * 60 * 24},
        {QnTimeStrings::Suffix::Hours, 60 * 60},
        {QnTimeStrings::Suffix::Minutes, 60}
    };

    for (const auto& [suffix, mult]: multipliers)
    {
        const auto value = seconds / mult;

        if (value != 0 && seconds == value * mult)
            return nx::format("%1 %2", value, QnTimeStrings::fullSuffix(suffix, value));
    }

    return nx::format("%1 %2",
        seconds,
        QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Seconds, seconds));
}

} // namespace nx::vms::client::desktop
