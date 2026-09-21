// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "push_notification_structures.h"

namespace nx::vms::client::mobile::details {

/** Falls back to a case-insensitive lookup for the entries saved before the normalization. */
OptionalLocalPushSettings findUserPushSettings(
    const UserPushSettings& settings, const QString& user);

/** Stores or removes (if the value is empty) the settings, dropping any non-normalized entries. */
bool updateUserPushSettings(
    UserPushSettings& settings, const QString& user, const OptionalLocalPushSettings& value);

} // namespace nx::vms::client::mobile::details
