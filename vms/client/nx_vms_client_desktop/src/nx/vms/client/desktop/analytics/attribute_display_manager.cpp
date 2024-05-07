// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_display_manager.h"

#include <nx/vms/client/desktop/analytics/analytics_filter_model.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/desktop/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

QString kDateTimeAttributeName = "^/DATE_TIME/^";
QString kCameraAttributeName = "^/CAMERA/^";
QString kObjectTypeAttributeName = "^/OBJECT_TYPE/^";
QStringList kBuiltInAttributes = {
    kDateTimeAttributeName,
    kCameraAttributeName,
    kObjectTypeAttributeName,
};

class AttributeDisplayManager::Private: public QObject
{
    AttributeDisplayManager* q = nullptr;

public:
    Private(Mode mode, AttributeDisplayManager* q):
        q(q),
        settingsProperty(mode == Mode::tileView
            ? &appContext()->localSettings()->analyticsSearchTileVisibleAttributes
            : &appContext()->localSettings()->analyticsSearchTableVisibleAttributes),
        mode(mode)
    {
    }

    void load()
    {
        for (const LocalSettings::AnalyticsAttributeSettings& attribute: settingsProperty->value())
        {
            allAttributes.insert(attribute.name);
            orderedAttributes.append(attribute.name);
            if (attribute.visible)
                visibleAttributes.insert(attribute.name);
        }

        for (auto it = kBuiltInAttributes.rbegin(); it != kBuiltInAttributes.rend(); ++it)
        {
            const QString& attribute = *it;

            if (mode == Mode::tileView)
            {
                // In the tile mode, built-in attributes go first.
                orderedAttributes.removeOne(attribute);
                orderedAttributes.prepend(attribute);
            }

            if (!allAttributes.contains(attribute))
            {
                allAttributes.insert(attribute);
                // Built-in attributes are visible by default.
                visibleAttributes.insert(attribute);
            }
        }

        QSignalBlocker blocker(q);
        handleObjectTypesChanged();
    }

    void save()
    {
        QList<LocalSettings::AnalyticsAttributeSettings> attributes;
        for (const QString& attribute: orderedAttributes)
        {
            attributes.push_back(LocalSettings::AnalyticsAttributeSettings{
                .name = attribute,
                .visible = visibleAttributes.contains(attribute)});
        }
        settingsProperty->setValue(attributes);
    }

    QStringList attributesForObjectType(const ObjectType* objectType)
    {
        QStringList result;

        for (const auto& attribute: objectType->attributes())
        {
            result.append(attribute->name);

            if (attribute->attributeSet)
            {
                for (const auto& subAttribute: attribute->attributeSet->attributes())
                    result.append(QString("%1 %2").arg(attribute->name, subAttribute->name));
            }
        }

        return result;
    }

    void handleObjectTypesChanged()
    {
        const auto oldCount = allAttributes.size();

        for (const auto& objectType: analyticsFilterModel->objectTypes())
        {
            for (const auto& attribute: attributesForObjectType(objectType))
            {
                if (!allAttributes.contains(attribute))
                {
                    allAttributes.insert(attribute);
                    orderedAttributes.append(attribute);
                }
            }
        }

        if (oldCount < allAttributes.size())
            emit q->attributesChanged();
    }

public:
    decltype(LocalSettings::analyticsSearchTileVisibleAttributes)* const settingsProperty;
    AnalyticsFilterModel* analyticsFilterModel = nullptr;
    const Mode mode = Mode::tileView;

    QSet<QString> visibleAttributes;
    QSet<QString> allAttributes;
    QStringList orderedAttributes;
    QMap<QString, QString> displayNameByAttribute;
};

AttributeDisplayManager::AttributeDisplayManager(
    Mode mode,
    AnalyticsFilterModel* filterModel)
    :
    QObject(filterModel),
    d(new Private(mode, this))
{
    d->analyticsFilterModel = filterModel;

    d->displayNameByAttribute = {
        {kDateTimeAttributeName, tr("Date/Time")},
        {kCameraAttributeName, tr("Camera")},
        {kObjectTypeAttributeName, tr("Object Type")},
    };

    connect(d->analyticsFilterModel, &AnalyticsFilterModel::objectTypesChanged,
        d.get(), &Private::handleObjectTypesChanged);

    d->load();
}

AttributeDisplayManager::~AttributeDisplayManager()
{
}

QStringList AttributeDisplayManager::attributes() const
{
    return d->orderedAttributes;
}

QStringList AttributeDisplayManager::builtInAttributes() const
{
    return kBuiltInAttributes;
}

QSet<QString> AttributeDisplayManager::visibleAttributes() const
{
    return d->visibleAttributes;
}

QString AttributeDisplayManager::displayName(const QString& attribute) const
{
    return d->displayNameByAttribute.value(attribute, attribute);
}

void AttributeDisplayManager::setVisible(const QString& attribute, bool visible)
{
    if (attribute == kDateTimeAttributeName)
        return; //< This attribute is not allowed to be hidden.

    if (!canBeHidden(attribute))
        return;

    if (!d->allAttributes.contains(attribute))
        return;

    const bool currentlyVisible = d->visibleAttributes.contains(attribute);
    if (currentlyVisible == visible)
        return;

    if (visible)
        d->visibleAttributes.insert(attribute);
    else
        d->visibleAttributes.remove(attribute);

    d->save();

    emit attributeVisibilityChanged(attribute);
    emit visibleAttributesChanged();

    if (attribute == kCameraAttributeName)
        emit cameraVisibleChanged();
    else if (attribute == kObjectTypeAttributeName)
        emit objectTypeVisibleChanged();
}

bool AttributeDisplayManager::canBeHidden(const QString& attribute) const
{
    return attribute != kDateTimeAttributeName;
}

bool AttributeDisplayManager::canBeMoved(const QString& attribute) const
{
    return (d->mode == Mode::tableView) || !kBuiltInAttributes.contains(attribute);
}

void AttributeDisplayManager::placeAttributeBefore(
    const QString& attribute, const QString& anchorAttribute)
{
    if (attribute == anchorAttribute)
        return;

    if (!canBeMoved(attribute) || !canBeMoved(anchorAttribute))
        return;

    int indexFrom = d->orderedAttributes.indexOf(attribute);
    int indexTo = d->orderedAttributes.indexOf(anchorAttribute);

    if (indexFrom < 0 || indexTo < 0)
        return;

    d->orderedAttributes.move(indexFrom, indexTo);
    d->save();

    emit attributesChanged();
}

QStringList AttributeDisplayManager::attributesForObjectType(
    const QStringList& objectTypeIds) const
{
    const ObjectType* objectType = d->analyticsFilterModel->findFilterObjectType(objectTypeIds);
    if (!objectType)
        return {};

    return d->attributesForObjectType(objectType);
}

bool AttributeDisplayManager::cameraVisible() const
{
    return isVisible(kCameraAttributeName);
}

bool AttributeDisplayManager::objectTypeVisible() const
{
    return isVisible(kObjectTypeAttributeName);
}

bool AttributeDisplayManager::isVisible(const QString& attribute) const
{
    return d->visibleAttributes.contains(attribute);
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
