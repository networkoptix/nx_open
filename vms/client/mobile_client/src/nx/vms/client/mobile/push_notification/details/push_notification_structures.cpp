// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_structures.h"

#include <QtCore/QtGlobal>

namespace nx::vms::client::mobile::details {

bool isExpertMode(const SystemSet& systems)
{
    return systems != allSystemsModeValue();
}

SystemSet allSystemsModeValue()
{
    return SystemSet({"all"});
}

//-------------------------------------------------------------------------------------------------

LocalPushSettings::LocalPushSettings():
    LocalPushSettings(makeLoggedOut())
{
}

LocalPushSettings::LocalPushSettings(
    bool enabled,
    const SystemSet& systems,
    const TokenData& tokenData)
    :
    enabled(enabled),
    systems(systems),
    tokenData(tokenData)
{
}

LocalPushSettings LocalPushSettings::makeCopyWithTokenData(
    const LocalPushSettings& what,
    const TokenData& data)
{
    return LocalPushSettings(what.enabled, what.systems, data);
}

LocalPushSettings LocalPushSettings::makeLoggedOut()
{
    return LocalPushSettings(/*enabled*/ false, allSystemsModeValue(), TokenData::makeEmpty());
}

LocalPushSettings LocalPushSettings::makeFromDeprecated(const LocalPushSettingsDeprecated& value)
{
    return LocalPushSettings(value.enabled, value.systems, TokenData::makeFirebase(value.token));
}

} // namespace nx::vms::client::mobile::details
