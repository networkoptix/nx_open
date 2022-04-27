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
                    attributePathByName[name] = path;

                    if (attribute->type() == AbstractAttribute::Type::object)
                        addAttributes(attribute->objectType(), path, name + ".");
                }
            });

        if (const auto objectType = taxonomy->objectTypeById(objectTypeId))
            addAttributes(objectType);

        return attributeLookup.insert(objectTypeId, attributePathByName).value();
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
    return d->ensureAttributeLookup(objectTypeId.trimmed()).value(attributeName.trimmed());
}

GroupedAttributes AttributeHelper::preprocessAttributes(
    const QString& objectTypeId, const Attributes& sourceAttributes) const
{
    if (objectTypeId.isEmpty())
        return groupAttributes(sourceAttributes, /*humanReadableNames*/ true);

    Attributes preprocessed;
    Attributes unknown;

    for (auto& nameValuePair: sourceAttributes)
    {
        if (nameValuePair.name.isEmpty())
            continue;

        const auto path = attributePath(objectTypeId, nameValuePair.name);
        if (path.isEmpty()) //< An attribute unknown to the current taxonomy.
        {
            addAttributeIfNotExists(&unknown, nameValuePair);
            continue;
        }

        QStringList name;
        for (const auto attribute: path)
            name.push_back(attribute->name());

        QString value = nameValuePair.value;
        switch (path.last()->type())
        {
            case AbstractAttribute::Type::boolean:
                value = QnLexical::deserialized<bool>(value) ? "Yes" : "No";
                break;

            case AbstractAttribute::Type::object:
                value = QnLexical::deserialized<bool>(value) ? "Present" : "Absent";
                break;

            default:
                break;
        }
        addAttributeIfNotExists(&preprocessed, { name.join(' '), value });
    }

    return groupAttributes(preprocessed, /*humanReadableNames*/ false)
        + groupAttributes(unknown, /*humanReadableNames*/ true);
}

} // namespace nx::vms::client::desktop::analytics
