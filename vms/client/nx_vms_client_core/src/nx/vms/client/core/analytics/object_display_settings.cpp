// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_display_settings.h"

#include <QtCore/QRegularExpression>

#include <nx/analytics/analytics_attributes.h>
#include <nx/kit/utils.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::core {

const std::map<QString, QString> ObjectDisplaySettings::kBoundingBoxPalette{
    {"Magenta", "#E040FB"},
    {"Blue", "#536DFE"},
    {"Green", "#B2FF59"},
    {"Yellow", "#FFFF00"},
    {"Cyan", "#18FFFF"},
    {"Purple", "#7C4DFF"},
    {"Orange", "#FFAB40"},
    {"Red", "#FF4081"},
    {"White", "#FFFFFF"},
};

ObjectDisplaySettings::ObjectDisplaySettings()
{
    m_settingsMap = appContext()->coreSettings()->detectedObjectSettings();
}

QColor ObjectDisplaySettings::objectColor(const QString& objectTypeId)
{
    if (const auto settingsIt = m_settingsMap.find(objectTypeId);
        settingsIt != m_settingsMap.end() && settingsIt->second.color.isValid())
    {
        return settingsIt->second.color;
    }

    // Color not found - select random color and update settings.
    m_settingsMap = appContext()->coreSettings()->detectedObjectSettings();
    auto& settings = m_settingsMap[objectTypeId];
    settings.color = nx::utils::random::choice(core::colorTheme()->colors("detectedObject"));
    appContext()->coreSettings()->detectedObjectSettings = m_settingsMap;

    return settings.color;
}

QColor ObjectDisplaySettings::objectColor(const nx::common::metadata::ObjectMetadata& object)
{
    static const QRegularExpression kHexColorRe("#[A-Fa-f0-9]{6}");

    for (const auto& attribute: object.attributes)
    {
        if (attribute.name != "nx.sys.color")
            continue;

        const auto& value = attribute.value;

        if (kHexColorRe.match(value).hasMatch())
            return QColor(value);

        const auto paletteEntry = kBoundingBoxPalette.find(value);

        if (paletteEntry == kBoundingBoxPalette.end())
        {
            // The specified color is missing in the palette; ignore the attribute.
            NX_DEBUG(this, "Analytics: Invalid nx.sys.color = %1 in ObjectMetadata %2%3",
                nx::kit::utils::toString(attribute.value),
                object.typeId,
                object.trackId);
            break;
        }

        return paletteEntry->second;
    }

    return objectColor(object.typeId);
}

std::vector<nx::common::metadata::Attribute> ObjectDisplaySettings::visibleAttributes(
    const nx::common::metadata::ObjectMetadata& object) const
{
    std::vector<nx::common::metadata::Attribute> result;
    for (const auto& attribute: object.attributes)
    {
        if (!nx::analytics::isAnalyticsAttributeHidden(attribute.name))
            result.push_back(attribute);
    }
    return result;
}

} // namespace nx::vms::client::core
