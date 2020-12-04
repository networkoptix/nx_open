#include "object_display_settings.h"

#include <regex>

#include <QtCore/QHash>

#include <nx/kit/utils.h>
#include <nx/utils/random.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <client/client_settings.h>
#include <nx/analytics/analytics_attributes.h>

namespace nx::vms::client::desktop {

namespace  {

struct ObjectDisplaySettingsItem
{
    enum class AttributeVisibilityPolicy
    {
        always,
        hover,
        never
    };

    QColor color;
    QHash<QString, AttributeVisibilityPolicy> attributeVisibility;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectDisplaySettingsItem, (json), (color)(attributeVisibility))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ObjectDisplaySettingsItem, AttributeVisibilityPolicy,
    (ObjectDisplaySettingsItem::AttributeVisibilityPolicy::always, "always")
    (ObjectDisplaySettingsItem::AttributeVisibilityPolicy::hover, "hover")
    (ObjectDisplaySettingsItem::AttributeVisibilityPolicy::never, "never"))

} // namespace

class ObjectDisplaySettings::Private
{
public:
    QHash<QString, ObjectDisplaySettingsItem> settingsByObjectTypeId;

    void loadSettings()
    {
        settingsByObjectTypeId = QJson::deserialized<decltype(settingsByObjectTypeId)>(
            qnSettings->detectedObjectDisplaySettings().toUtf8());
    }

    void saveSettings()
    {
        qnSettings->setDetectedObjectDisplaySettings(
            QString::fromUtf8(QJson::serialized(settingsByObjectTypeId)));
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
        settings.color = utils::random::choice(colorTheme()->groupColors("detectedObject"));
        d->saveSettings();
    }
    return settings.color;
}

/**
 * Palette used for nx.sys.color attribute value.
 * TODO: ATTENTION: These color names are coupled with hanwha_analytics_plugin.
 */
static const std::map</*name*/ std::string, /*hexRgb*/ std::string> kBoundingBoxPalette{
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

std::vector<nx::common::metadata::Attribute> ObjectDisplaySettings::briefAttributes(
    const nx::common::metadata::ObjectMetadata& object) const
{
    const auto& settings = d->settingsByObjectTypeId.value(object.typeId);

    std::vector<nx::common::metadata::Attribute> result;

    for (const auto& attribute: object.attributes)
    {
        if (!nx::analytics::isAnalyticsAttributeHidden(attribute.name)
            && settings.attributeVisibility.value(attribute.name)
                == ObjectDisplaySettingsItem::AttributeVisibilityPolicy::always)
        {
            result.push_back(attribute);
        }
    }

    return result;
}

std::vector<nx::common::metadata::Attribute> ObjectDisplaySettings::visibleAttributes(
    const nx::common::metadata::ObjectMetadata& object) const
{
    const auto& settings = d->settingsByObjectTypeId.value(object.typeId);

    std::vector<nx::common::metadata::Attribute> result;

    for (const auto& attribute: object.attributes)
    {
        if (!nx::analytics::isAnalyticsAttributeHidden(attribute.name)
            && settings.attributeVisibility.value(attribute.name)
                != ObjectDisplaySettingsItem::AttributeVisibilityPolicy::never)
        {
            result.push_back(attribute);
        }
    }

    return result;
}

} // namespace nx::vms::client::desktop
