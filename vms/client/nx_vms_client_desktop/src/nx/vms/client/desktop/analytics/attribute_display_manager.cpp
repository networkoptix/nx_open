// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_display_manager.h"

#include <QtQml/QtQml>

#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute.h>
#include <nx/vms/client/core/analytics/taxonomy/attribute_set.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

const QStringList kBuiltInAttributes = {
    kDateTimeAttributeName,
    kTitleAttributeName,
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
        reset();
    }

    void load()
    {
        for (const LocalSettings::AnalyticsAttributeSettings& attribute: settingsProperty->value())
        {
            allAttributes.insert(attribute.id);
            orderedAttributes.append(attribute.id);
            if (attribute.visible)
                visibleAttributes.insert(attribute.id);
        }

        for (auto it = kBuiltInAttributes.rbegin(); it != kBuiltInAttributes.rend(); ++it)
        {
            const QString& attribute = *it;

            if (mode == Mode::tileView)
            {
                // In the tile mode, built-in attributes always go first.
                orderedAttributes.removeOne(attribute);
                if (attribute != kObjectTypeAttributeName)
                    orderedAttributes.prepend(attribute);
            }
            else
            {
                // In the table mode, built-in attributes go first by default.
                if (!orderedAttributes.contains(attribute))
                {
                    int pos = 0;
                    if (const auto nextIt = it + 1; nextIt != kBuiltInAttributes.rend())
                        pos = orderedAttributes.indexOf(*nextIt) + 1;

                    orderedAttributes.insert(pos, attribute);
                }
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
        attributeIndexesDirty = true;
    }

    void save()
    {
        QList<LocalSettings::AnalyticsAttributeSettings> attributes;
        for (const QString& attribute: orderedAttributes)
        {
            if (mode == Mode::tileView && attribute == kObjectTypeAttributeName)
                continue;
            attributes.push_back(LocalSettings::AnalyticsAttributeSettings{
                .id = attribute,
                .visible = visibleAttributes.contains(attribute)});
        }
        settingsProperty->setValue(attributes);
    }

    QStringList attributesForObjectType(const core::analytics::taxonomy::ObjectType* objectType) const
    {
        return objectAttributes.value(objectType, {});
    }

    void handleObjectTypesChanged()
    {
        reset();
        const auto oldCount = allAttributes.size();
        for (const auto& objectType: analyticsFilterModel->objectTypes())
        {
            fetchAttributesFromObjectType(objectType);
            for (const auto& attribute: attributesForObjectType(objectType))
            {
                if (!allAttributes.contains(attribute))
                {
                    allAttributes.insert(attribute);
                    orderedAttributes.append(attribute);
                    attributeIndexesDirty = true;
                }
            }
        }

        if (oldCount < allAttributes.size())
            emit q->attributesChanged();
    }

    void fetchAttributesFromObjectType(
        const core::analytics::taxonomy::ObjectType* objectType)
    {
        QStringList idList;

        auto fetch = nx::utils::y_combinator(
            [this, &idList](auto fetch, auto attributes, const QStringList& parentIds)->void
            {
                for (const auto& attribute: attributes)
                {
                    const QString id = attribute->name;
                    QStringList itemIds(parentIds);
                    itemIds.append(id);
                    const QString fullId = itemIds.join(".");
                    idList.append(fullId);
                    if (!parentIds.isEmpty())
                        displayNameByAttribute.insert(fullId, itemIds.join(" "));
                    if (attribute->attributeSet)
                        fetch(attribute->attributeSet->attributes(), itemIds);
                }
            });

        fetch(objectType->attributes(), QStringList());
        objectAttributes.insert(objectType, idList);
    }

    void reset()
    {
        displayNameByAttribute = {
            {kDateTimeAttributeName, tr("Date/Time")},
            {kTitleAttributeName, tr("Title")},
            {kCameraAttributeName, tr("Camera")},
            {kObjectTypeAttributeName, tr("Object Type")},
        };
        objectAttributes.clear();
    }

public:
    decltype(LocalSettings::analyticsSearchTileVisibleAttributes)* const settingsProperty;
    core::analytics::taxonomy::AnalyticsFilterModel* analyticsFilterModel = nullptr;
    const Mode mode = Mode::tileView;

    QSet<QString> visibleAttributes;
    QSet<QString> allAttributes;
    QStringList orderedAttributes;
    QMap<QString, QString> displayNameByAttribute;
    QHash<const core::analytics::taxonomy::ObjectType*, QStringList> objectAttributes;
    mutable QHash<QString, int> attributeIndexes;
    mutable bool attributeIndexesDirty = false;
};

void AttributeDisplayManager::registerQmlType()
{
    qmlRegisterType<taxonomy::AttributeDisplayManager>(
        "nx.vms.client.desktop.analytics", 1, 0, "AttributeDisplayManager");

    qmlRegisterSingletonType<details::Factory>(
        "nx.vms.client.desktop", 1, 0, "AttributeDisplayManagerFactory",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/)
        {
            return new details::Factory();
        });
}

AttributeDisplayManager::AttributeDisplayManager(
    Mode mode,
    core::analytics::taxonomy::AnalyticsFilterModel* filterModel)
    :
    QObject(filterModel),
    d(new Private(mode, this))
{
    d->analyticsFilterModel = filterModel;

    connect(d->analyticsFilterModel,
        &core::analytics::taxonomy::AnalyticsFilterModel::objectTypesChanged,
        d.get(),
        &Private::handleObjectTypesChanged);

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
    return attribute != kDateTimeAttributeName
        && (d->mode == Mode::tableView || attribute != kTitleAttributeName);
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

    d->attributeIndexesDirty = true;
    emit attributesChanged();
}

QStringList AttributeDisplayManager::attributesForObjectType(
    const QStringList& objectTypeIds) const
{
    const core::analytics::taxonomy::ObjectType* objectType =
        d->analyticsFilterModel->findFilterObjectType(objectTypeIds);
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

QHash<QString, int> AttributeDisplayManager::attributeIndexes() const
{
    if (!d->attributeIndexesDirty)
        return d->attributeIndexes;

    d->attributeIndexes.clear();

    int i = 0;
    for (const auto& attribute: d->orderedAttributes)
        d->attributeIndexes[attribute] = i++;

    d->attributeIndexesDirty = false;
    return d->attributeIndexes;
}

namespace details {

AttributeDisplayManager* Factory::create(
    AttributeDisplayManager::Mode mode,
    core::analytics::taxonomy::AnalyticsFilterModel* filterModel)
{
    return new AttributeDisplayManager(mode, filterModel);
}

} // namespace details

} // namespace nx::vms::client::desktop::analytics::taxonomy
