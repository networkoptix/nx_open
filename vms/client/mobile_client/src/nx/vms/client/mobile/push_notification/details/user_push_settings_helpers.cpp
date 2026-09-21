// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_push_settings_helpers.h"

#include <algorithm>

namespace nx::vms::client::mobile::details {

static auto sameUserPredicate(const QString& user)
{
    return [&user](const auto& item)
    {
        return item.first.compare(user, Qt::CaseInsensitive) == 0;
    };
}

OptionalLocalPushSettings findUserPushSettings(
    const UserPushSettings& settings, const QString& user)
{
    if (user.isEmpty())
        return std::nullopt;

    auto it = settings.find(user.toLower());
    if (it == settings.cend())
        it = std::ranges::find_if(settings, sameUserPredicate(user));

    return it == settings.cend() ? std::nullopt : OptionalLocalPushSettings(it->second);
}

bool updateUserPushSettings(
    UserPushSettings& settings, const QString& user, const OptionalLocalPushSettings& value)
{
    if (user.isEmpty())
        return false;

    const bool removed = std::erase_if(settings, sameUserPredicate(user)) > 0;
    if (!value)
        return removed;

    settings[user.toLower()] = *value;
    return true;
}

} // namespace nx::vms::client::mobile::details
