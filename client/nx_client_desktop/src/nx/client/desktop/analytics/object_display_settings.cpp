#include "object_display_settings.h"

#include <QtCore/QHash>

#include <nx/utils/random.h>

namespace nx {
namespace client {
namespace desktop {

namespace  {

static const QVector<QColor> kFrameColors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::darkRed,
    Qt::darkGreen,
    Qt::darkBlue,
    Qt::darkCyan,
    Qt::darkMagenta,
    Qt::darkYellow
};

enum class AttributeVisibilityPolicy
{
    always,
    hover,
    never
};

struct ObjectDisplaySettingsItem
{
    QColor color;
    QHash<QString, AttributeVisibilityPolicy> attributeVisibility;
};

} // namespace

class ObjectDisplaySettings::Private
{
public:
    QHash<QnUuid, ObjectDisplaySettingsItem> settingsByTypeId;
};

ObjectDisplaySettings::ObjectDisplaySettings():
    d(new Private())
{
    // TODO: #dklychkov Implement saving settings.
}

ObjectDisplaySettings::~ObjectDisplaySettings()
{
}

QColor ObjectDisplaySettings::objectColor(const QnUuid& objectTypeId)
{
    auto& settings = d->settingsByTypeId[objectTypeId];
    if (!settings.color.isValid())
        settings.color = utils::random::choice(kFrameColors);
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
        if (settings.attributeVisibility.value(attribute.name, AttributeVisibilityPolicy::always)
            == AttributeVisibilityPolicy::always)
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
        if (settings.attributeVisibility.value(attribute.name, AttributeVisibilityPolicy::always)
            != AttributeVisibilityPolicy::never)
        {
            result.push_back(attribute);
        }
    }

    return result;}

} // namespace desktop
} // namespace client
} // namespace nx
