// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::rules::utils {

static constexpr auto kCaptionDetailName = "caption";
static constexpr auto kDescriptionDetailName = "description";
static constexpr auto kNameDetailName = "name";
static constexpr auto kTimestampDetailName = "timestamp";
static constexpr auto kSourceDetailName = "source";
static constexpr auto kPluginDetailName = "plugin";
static constexpr auto kDetailingDetailName = "detailing";

inline void insertIfNotEmpty(QMap<QString, QString>& map, const QString& key, const QString& value)
{
    if (!value.isEmpty())
        map.insert(key, value);
}

} // namespace nx::vms::rules::utils
