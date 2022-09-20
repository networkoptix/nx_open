// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "node.h"

#include <map>
#include <optional>

#include <QtCore/QPointer>

#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/common.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute_set.h>

#include "utils.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

static bool objectTypeMatchesFilter(
    const AbstractStateViewFilter* filter,
    const nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    return (!filter || filter->matches(objectType)) && objectType->hasEverBeenSupported();
}

struct Node::Private
{
    const nx::analytics::taxonomy::AbstractObjectType* const mainObjectType = nullptr;
    std::map<QString, const nx::analytics::taxonomy::AbstractObjectType*> additionalObjectTypes;
    mutable std::optional<std::vector<QString>> cachedTypeIds;
    mutable std::optional<std::vector<QString>> cachedFullSubtreeTypeIds;
    mutable std::optional<QString> cachedId;
    QPointer<AbstractNode> baseNode;
    std::vector<AbstractNode*> derivedNodes;
    std::vector<AbstractAttribute*> attributes;
    const AbstractStateViewFilter* filter = nullptr;
};

Node::Node(const nx::analytics::taxonomy::AbstractObjectType* mainObjectType, QObject* parent):
    AbstractNode(parent),
    d(new Private{.mainObjectType = mainObjectType})
{
    NX_ASSERT(d->mainObjectType);
}

Node::~Node()
{
    // Required here for forward-declared scoped pointer destruction.
}

void Node::addDerivedNode(AbstractNode* node)
{
    if (!NX_ASSERT(node))
        return;

    NX_ASSERT(node->baseNode() == nullptr);

    node->setBaseNode(this);
    d->derivedNodes.push_back(node);
}

void Node::addObjectType(const nx::analytics::taxonomy::AbstractObjectType* objectType)
{
    if (!NX_ASSERT(objectType))
        return;

    d->additionalObjectTypes[objectType->id()] = objectType;
}

std::vector<QString> Node::typeIds() const
{
    if (d->cachedTypeIds)
        return *d->cachedTypeIds;

    d->cachedTypeIds = std::vector<QString>();
    if (objectTypeMatchesFilter(d->filter, d->mainObjectType))
        d->cachedTypeIds->push_back(d->mainObjectType->id());

    for (const auto& [objectTypeId, objectType]: d->additionalObjectTypes)
    {
        if (objectTypeMatchesFilter(d->filter, objectType))
            d->cachedTypeIds->push_back(objectTypeId);
    }

    return *d->cachedTypeIds;
}

QString Node::mainTypeId() const
{
    return d->mainObjectType->id();
}

std::vector<QString> Node::fullSubtreeTypeIds() const
{
    if (d->cachedFullSubtreeTypeIds)
        return *d->cachedFullSubtreeTypeIds;

    std::set<QString> result;
    for (const AbstractNode* derivedNode: d->derivedNodes)
    {
        std::vector<QString> derivedNodeSubtreeTypeIds = derivedNode->fullSubtreeTypeIds();
        result.insert(derivedNodeSubtreeTypeIds.begin(), derivedNodeSubtreeTypeIds.end());
    }

    std::vector<QString> ownTypeIds = this->typeIds();
    result.insert(ownTypeIds.begin(), ownTypeIds.end());

    d->cachedFullSubtreeTypeIds = std::vector<QString>(result.begin(), result.end());
    return *d->cachedFullSubtreeTypeIds;
}

QString Node::id() const
{
    if (d->cachedId)
        return *d->cachedId;

    const std::vector<QString> ids = fullSubtreeTypeIds();
    d->cachedId = makeId(QStringList{ids.begin(), ids.end()});
    return *d->cachedId;
}

QString Node::name() const
{
    if (NX_ASSERT(d->mainObjectType))
        return d->mainObjectType->name();

    return QString();
}

QString Node::icon() const
{
    if (NX_ASSERT(d->mainObjectType))
        return d->mainObjectType->icon();

    return QString();
}

std::vector<AbstractAttribute*> Node::attributes() const
{
    return d->attributes;
}

AbstractNode* Node::baseNode() const
{
    return d->baseNode;
}

std::vector<AbstractNode*> Node::derivedNodes() const
{
    return d->derivedNodes;
}

void Node::setFilter(const AbstractStateViewFilter* filter)
{
    d->filter = filter;
}

void Node::resolveAttributes()
{
    std::vector<const nx::analytics::taxonomy::AbstractObjectType*> objectTypes;

    if (objectTypeMatchesFilter(d->filter, d->mainObjectType))
        objectTypes.push_back(d->mainObjectType);

    for (const auto& [_, objectType]: d->additionalObjectTypes)
    {
        if (objectTypeMatchesFilter(d->filter, objectType))
            objectTypes.push_back(objectType);
    }

    d->attributes = taxonomy::resolveAttributes(objectTypes, d->filter, this);
}

QString Node::makeId(const QStringList& analyticsObjectTypeIds)
{
    return analyticsObjectTypeIds.join("|");
}

void Node::setBaseNode(AbstractNode* node)
{
    d->baseNode = node;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
