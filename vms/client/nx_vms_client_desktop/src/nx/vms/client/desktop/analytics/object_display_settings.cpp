// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_display_settings.h"

#include <regex>
#include <unordered_map>

#include <client/client_settings.h>
#include <nx/analytics/analytics_attributes.h>
#include <nx/kit/utils.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/common/serialization/qt_gui_types.h>

namespace nx::vms::client::desktop {

namespace  {

struct ObjectDisplaySettingsItem
{
    QColor color;
};

NX_REFLECTION_INSTRUMENT(ObjectDisplaySettingsItem, (color))

} // namespace

const std::map<std::string, std::string> ObjectDisplaySettings::kBoundingBoxPalette{
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

class ObjectDisplaySettings::Private
{
public:
    std::unordered_map<QString, ObjectDisplaySettingsItem> settingsByObjectTypeId;

    void loadSettings()
    {
        nx::reflect::json::deserialize(
            qnSettings->detectedObjectDisplaySettings().toUtf8().data(), &settingsByObjectTypeId);
    }

    void saveSettings()
    {
        qnSettings->setDetectedObjectDisplaySettings(
            QString::fromStdString(nx::reflect::json::serialize(settingsByObjectTypeId)));
    }
};

ObjectDisplaySettings::ObjectDisplaySettings():
    d(new Private())
{
    d->loadSettings();
}

ObjectDisplaySettings::~ObjectDisplaySettings()
{
}

QColor ObjectDisplaySettings::objectColor(const QString& objectTypeId)
{
    auto& settings = d->settingsByObjectTypeId[objectTypeId];
    if (!settings.color.isValid())
    {
        settings.color = utils::random::choice(colorTheme()->colors("detectedObject"));
        d->saveSettings();
    }
    return settings.color;
}

QColor ObjectDisplaySettings::objectColor(const nx::common::metadata::ObjectMetadata& object)
{
    for (const auto& attribute: object.attributes)
    {
        if (attribute.name != "nx.sys.color")
            continue;

        std::string value = attribute.value.toStdString();

        if (!std::regex_match(value, std::regex("#[A-Fa-f0-9]{6}")))
        {
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

            value = paletteEntry->second;
        }

        return QColor(QString::fromLatin1(value.c_str()));
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

} // namespace nx::vms::client::desktop
