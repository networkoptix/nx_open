// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <optional>
#include <set>
#include <unordered_map>

#include <nx/reflect/instrument.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/uuid.h>

#include "token_data.h"

namespace nx::vms::client::mobile::details {

NX_REFLECTION_ENUM_CLASS(OsType,
    ios,
    android,
    genericDesktop //< Value for build only purposes.
)

struct DeviceInfo
{
    QString name;
    QString model;
    OsType os = OsType::genericDesktop;
};
#define DeviceInfo_Fields (name)(model)(os)
NX_REFLECTION_INSTRUMENT(DeviceInfo, DeviceInfo_Fields)

using SystemSet = std::set<QString>;

bool isExpertMode(const SystemSet& systems);
SystemSet allSystemsModeValue();

/** Deprecated in mobile client 21.2 */
struct LocalPushSettingsDeprecated
{
    bool enabled = true;
    QString token;
    SystemSet systems;
};
NX_REFLECTION_INSTRUMENT(LocalPushSettingsDeprecated, (enabled)(systems)(token))

/**
 * Represents notifications settings. Confirmed ones are stored locally.
 */
struct LocalPushSettings
{
    /**
     * Shows if push notifications are currently enabled (when user is logged in into the cloud
     * already) or should be enabled (when user logs in to the cloud).
     */
    bool enabled = true;

    /**
     * Set of systems to operate with push notifications. Has special value (returned by
     * allSystemsModeValue function call) which indicates that we should allow push notifications
     * for all systems (including future new ones).
     */
    SystemSet systems = allSystemsModeValue();

    /**
     * Represents current token data used for push notification delivering process.
     * Invalid token means that the user was logged out from the cloud (himself or forcefully by
     * the cloud request) or he just logged in and hasn't turned push notifications on yet.
     * Valid token data means that current user has succesfully turned push notifications on
     * at least once previously (until he logged out from the cloud on device).
     */
    TokenData tokenData;

    LocalPushSettings();

    LocalPushSettings(
        bool enabled,
        const SystemSet& systems,
        const TokenData& tokenData);

    static LocalPushSettings makeCopyWithTokenData(
        const LocalPushSettings& what,
        const TokenData& data);

    static LocalPushSettings makeLoggedOut();

    static LocalPushSettings makeFromDeprecated(const LocalPushSettingsDeprecated& value);

    bool operator == (const LocalPushSettings& /*other*/) const = default;
};
NX_REFLECTION_INSTRUMENT(LocalPushSettings, (enabled)(systems)(tokenData))

using OptionalLocalPushSettings = std::optional<LocalPushSettings>;
using UserName = QString;
using UserPushSettings = std::unordered_map<UserName, LocalPushSettings>;
using UserPushSettingsDeprecated = QHash<UserName, LocalPushSettingsDeprecated>;

} // namespace nx::vms::client::mobile::details

Q_DECLARE_METATYPE(nx::vms::client::mobile::details::DeviceInfo)
Q_DECLARE_METATYPE(nx::vms::client::mobile::details::LocalPushSettings)
Q_DECLARE_METATYPE(nx::vms::client::mobile::details::LocalPushSettingsDeprecated)
Q_DECLARE_METATYPE(nx::vms::client::mobile::details::UserPushSettings)
