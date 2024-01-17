// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_attribute_helper.h"

#include <algorithm>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <common/common_module.h>
#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/std/algorithm.h>

using namespace nx::analytics::taxonomy;
using namespace nx::common::metadata;

namespace nx::vms::client::desktop::analytics {

class AttributeHelper::Private: public QObject
{
    const QPointer<AbstractStateWatcher> taxonomyStateWatcher;

    using AttributePathByName = QHash<QString /*attributeName*/, AttributePath>;
    mutable QHash<QString /*objectTypeId*/, AttributePathByName> attributeLookup;

public:
    Private(AbstractStateWatcher* taxonomyStateWatcher):
        taxonomyStateWatcher(taxonomyStateWatcher)
    {
        if (!NX_ASSERT(taxonomyStateWatcher))
            return;

        connect(taxonomyStateWatcher, &AbstractStateWatcher::stateChanged, this,
            [this]() { attributeLookup.clear(); });
    }

    AttributePathByName ensureAttributeLookup(const QString& objectTypeId) const
    {
        const auto taxonomy = taxonomyStateWatcher ? taxonomyStateWatcher->state() : nullptr;
        if (!taxonomy)
            return {};

        const auto it = attributeLookup.find(objectTypeId);
        if (it != attributeLookup.end())
            return it.value();

        AttributePathByName attributePathByName;

        const auto addAttributes = nx::utils::y_combinator(
            [&attributePathByName](
                auto addAttributes,
                AbstractObjectType* objectType,
                const QVector<AbstractAttribute*>& pathPrefix = QVector<AbstractAttribute*>(),
                const QString& namePrefix = QString()) -> void
            {
                for (const auto attribute: objectType->attributes())
                {
                    const auto path = pathPrefix + QVector<AbstractAttribute*>({attribute});
                    const auto name = namePrefix + attribute->name();
                    attributePathByName[name.toLower()] = path;

                    if (attribute->type() == AbstractAttribute::Type::object)
                        addAttributes(attribute->objectType(), path, name + ".");
                }
            });

        if (const auto objectType = taxonomy->objectTypeById(objectTypeId))
            addAttributes(objectType);

        return attributeLookup.insert(objectTypeId, attributePathByName).value();
    }

    static QStringList processBooleanValues(
        const QStringList& sourceValues, const QString& trueValue, const QString& falseValue)
    {
        QStringList result;
        result.reserve(sourceValues.size());
        for (const auto& value: sourceValues)
            result.push_back(QnLexical::deserialized<bool>(value) ? trueValue : falseValue);
        return result;
    };

    static QColor findColor(AbstractColorType* colorType, const QString& color)
    {
        const std::vector<QString> items = colorType->items();
        const auto item = std::find_if(items.begin(), items.end(),
            [color](const QString& item){ return item.toLower() == color; });

        return item != items.end() ? QColor(colorType->color(*item)) : QColor::fromString(color);
    }

    QVector<QString> objectTypeIds() const
    {
        const std::shared_ptr<AbstractState> taxonomy =
            taxonomyStateWatcher ? taxonomyStateWatcher->state() : nullptr;

        if (!taxonomy)
            return {};

        QVector<QString> result;
        for (const AbstractObjectType* objectType: taxonomy->objectTypes())
            result.append(objectType->id());

        return result;
    }
};

AttributeHelper::AttributeHelper(AbstractStateWatcher* taxonomyStateWatcher):
    d(new Private(taxonomyStateWatcher))
{
}

AttributeHelper::~AttributeHelper()
{
    // Required here for forward-declared scoped pointer destruction.
}

QVector<AbstractAttribute*> AttributeHelper::attributePath(
    const QString& objectTypeId, const QString& attributeName) const
{
    return d->ensureAttributeLookup(objectTypeId.trimmed()).value(
        attributeName.trimmed().toLower());
}

nx::analytics::taxonomy::AbstractAttribute* AttributeHelper::findAttribute(
    const QString& attributeName,
    const QVector<QString>& objectTypeIds) const
{
    for (const QString& objectTypeId: objectTypeIds.isEmpty() ? d->objectTypeIds() : objectTypeIds)
    {
        QVector<AbstractAttribute*> path = attributePath(objectTypeId, attributeName);
        if (!path.isEmpty())
            return path.last();
    }

    return nullptr;
}

AttributeList AttributeHelper::preprocessAttributes(
    const QString& objectTypeId, const Attributes& sourceAttributes) const
{
    // humanReadableNames here means only normalization of "True" and "False" spelling.
    const auto grouped = groupAttributes(sourceAttributes, /*humanReadableNames*/ true);

    QList<Attribute> result;

    if (objectTypeId.isEmpty())
    {
        result.reserve(grouped.size());
        for (const auto& attr: grouped)
        {
            if (NX_ASSERT(!attr.name.isEmpty()))
                result.push_back({attr.name, attr.values, attr.name, attr.values});
        }
        return result;
    }

    AttributeList known;
    AttributeList unknown;

    for (const auto& attr: grouped)
    {
        if (!NX_ASSERT(!attr.name.isEmpty()))
            continue;

        const auto path = attributePath(objectTypeId, attr.name);
        if (path.isEmpty()) //< An attribute unknown to the current taxonomy.
        {
            unknown.push_back({attr.name, attr.values, attr.name, attr.values});
            continue;
        }

        QStringList nameParts;
        for (const auto attribute: path)
            nameParts.push_back(attribute->name());

        const QString name = nameParts.join(' ');

        switch (path.last()->type())
        {
            case AbstractAttribute::Type::boolean:
                known.push_back({attr.name, attr.values, name, Private::processBooleanValues(
                    attr.values, "Yes", "No")});
                break;

            case AbstractAttribute::Type::object:
                known.push_back({attr.name, attr.values, name, Private::processBooleanValues(
                    attr.values, "Present", "Absent")});
                break;

            case AbstractAttribute::Type::color:
            {
                const auto colorType = path.last()->colorType();
                QVariantList colors;
                for (const QString& value: attr.values)
                    colors.push_back(Private::findColor(colorType, value));

                known.push_back({attr.name, attr.values, name, attr.values, colors});
                break;
            }

            default:
                known.push_back({attr.name, attr.values, name, attr.values});
                break;
        }
    }

    return known + unknown;
}

} // namespace nx::vms::client::desktop::analytics
