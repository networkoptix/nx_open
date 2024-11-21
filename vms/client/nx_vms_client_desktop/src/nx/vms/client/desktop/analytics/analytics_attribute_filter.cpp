// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_attribute_filter.h"

#include <QtCore/QPointer>
#include <QtQml/QtQml>

#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop::analytics {

struct AttributeFilter::Private
{
    AttributeFilter* const q;
    QPointer<Manager> manager;
    nx::utils::ScopedConnections managerConnections;
    bool filter = true;
    bool sort = true;
    AttributeList attributes;
    AttributeList displayedAttributes;

    void update()
    {
        setDisplayedAttributes(calculateDisplayedAttributes());
    }

    void setDisplayedAttributes(const AttributeList& value)
    {
        if (displayedAttributes == value)
            return;

        displayedAttributes = value;
        emit q->displayedAttributesChanged();
    }

    AttributeList calculateDisplayedAttributes() const
    {
        if (!manager || !(filter || sort))
            return attributes;

        AttributeList result;
        if (filter)
        {
            result.reserve(attributes.size());
            for (const auto attribute: attributes)
            {
                if (manager->isVisible(attribute.id))
                    result.push_back(attribute);
            }
        }
        else
        {
            result = attributes;
        }

        if (sort)
        {
            std::sort(result.begin(), result.end(),
                [sortKeys = manager->attributeIndexes()](const Attribute& l, const Attribute& r)
                {
                    return sortKeys.value(l.id) < sortKeys.value(r.id);
                });
        }

        return result;
    }
};

AttributeFilter::AttributeFilter(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
}

AttributeFilter::~AttributeFilter()
{
    // Required here for forward-declared scoped pointer destruction.
}

void AttributeFilter::registerQmlType()
{
    qmlRegisterType<AttributeFilter>(
        "nx.vms.client.desktop.analytics", 1, 0, "AttributeFilter");
}

AttributeFilter::Manager* AttributeFilter::manager() const
{
    return d->manager;
}

void AttributeFilter::setManager(Manager* value)
{
    if (d->manager == value)
        return;

    d->managerConnections.reset();
    d->manager = value;

    if (d->manager)
    {
        d->managerConnections << connect(d->manager.get(), &QObject::destroyed, this,
            [this]() { d->update(); });

        d->managerConnections << connect(d->manager.get(), &Manager::attributesChanged, this,
            [this]() { d->update(); });

        d->managerConnections << connect(d->manager.get(), &Manager::visibleAttributesChanged, this,
            [this]() { d->update(); });
    }

    d->update();
    emit managerChanged();
}

AttributeFilter::AttributeList AttributeFilter::attributes() const
{
    return d->attributes;
}

void AttributeFilter::setAttributes(const AttributeList& value)
{
    if (d->attributes == value)
        return;

    d->attributes = value;
    d->update();
    emit attributesChanged();
}

bool AttributeFilter::filter() const
{
    return d->filter;
}

void AttributeFilter::setFilter(bool value)
{
    if (d->filter == value)
        return;

    d->filter = value;
    d->update();
    emit filterChanged();
}

bool AttributeFilter::sort() const
{
    return d->sort;
}

void AttributeFilter::setSort(bool value)
{
    if (d->sort == value)
        return;

    d->sort = value;
    d->update();
    emit sortChanged();
}

AttributeFilter::AttributeList AttributeFilter::displayedAttributes() const
{
    return d->displayedAttributes;
}

} // namespace nx::vms::client::desktop::analytics
