#include "object_display_settings.h"

#include <QtCore/QHash>

#include <nx/utils/random.h>
#include <nx/fusion/model_functions.h>
#include <nx/client/desktop/ui/common/color_theme.h>
#include <client/client_settings.h>

namespace nx {
namespace client {
namespace desktop {

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

bool systemAttribute(const QString& name)
{
    return name.startsWith(lit("nx.sys."));
}

} // namespace

class ObjectDisplaySettings::Private
{
public:
    QHash<QnUuid, ObjectDisplaySettingsItem> settingsByTypeId;

    void loadSettings()
    {
        settingsByTypeId = QJson::deserialized<decltype(settingsByTypeId)>(
            qnSettings->detectedObjectDisplaySettings().toUtf8());
    }

    void saveSettings()
    {
        qnSettings->setDetectedObjectDisplaySettings(
            QString::fromUtf8(QJson::serialized(settingsByTypeId)));
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

QColor ObjectDisplaySettings::objectColor(const QnUuid& objectTypeId)
{
    auto& settings = d->settingsByTypeId[objectTypeId];
    if (!settings.color.isValid())
    {
        settings.color = utils::random::choice(colorTheme()->groupColors("detectedObject"));
        d->saveSettings();
    }
    return settings.color;
}

QColor ObjectDisplaySettings::objectColor(const common::metadata::DetectedObject& object)
{
    return objectColor(object.objectTypeId);
}

std::vector<common::metadata::Attribute> ObjectDisplaySettings::briefAttributes(
    const common::metadata::DetectedObject& object) const
{
    const auto& settings = d->settingsByTypeId.value(object.objectTypeId);

    std::vector<common::metadata::Attribute> result;

    for (const auto& attribute: object.labels)
    {
        if (!systemAttribute(attribute.name) && settings.attributeVisibility.value(attribute.name)
            == ObjectDisplaySettingsItem::AttributeVisibilityPolicy::always)
        {
            result.push_back(attribute);
        }
    }

    return result;
}

std::vector<common::metadata::Attribute> ObjectDisplaySettings::visibleAttributes(
    const common::metadata::DetectedObject& object) const
{
    const auto& settings = d->settingsByTypeId.value(object.objectTypeId);

    std::vector<common::metadata::Attribute> result;

    for (const auto& attribute: object.labels)
    {
        if (!systemAttribute(attribute.name) && settings.attributeVisibility.value(attribute.name)
            != ObjectDisplaySettingsItem::AttributeVisibilityPolicy::never)
        {
            result.push_back(attribute);
        }
    }

    return result;
}

} // namespace desktop
} // namespace client
} // namespace nx
