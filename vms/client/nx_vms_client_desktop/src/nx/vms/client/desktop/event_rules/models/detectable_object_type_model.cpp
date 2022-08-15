// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detectable_object_type_model.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics::taxonomy;

class DetectableObjectTypeModel::Private
{
    DetectableObjectTypeModel* const q;
    QPointer<TaxonomyManager> manager;

public:
    Private(DetectableObjectTypeModel* q, TaxonomyManager* manager):
        q(q),
        manager(manager)
    {
        if (!NX_ASSERT(manager))
            return;

        QObject::connect(manager, &TaxonomyManager::currentTaxonomyAboutToBeChanged, q,
            [this]() { this->q->beginResetModel(); });

        QObject::connect(manager, &TaxonomyManager::currentTaxonomyChanged, q,
            [this]()
            {
                invalidateCache();
                this->q->endResetModel();
            });
    }

    struct TypeData
    {
        AbstractObjectType* type = nullptr;
        QVector<quintptr> derivedTypeIds;
        int indexInParent = -1;
    };

    TypeData data(quintptr id) const
    {
        ensureObjectTypeCache();
        return objectTypeCache.value(id);
    }

    void invalidateCache()
    {
        objectTypeCache.clear();
    }

    bool liveTypesExcluded = false;
    QPointer<AbstractEngine> engine;

private:
    void ensureObjectTypeCache() const
    {
        if (!objectTypeCache.empty() || !NX_ASSERT(manager))
            return;

        const auto currentTaxonomy = manager->currentTaxonomy();
        if (!currentTaxonomy)
            return;

        recursivelyAddDetectableTypes(/*parent*/ nullptr, /*indexInParent*/ -1,
            currentTaxonomy->rootObjectTypes());
    }

    void recursivelyAddDetectableTypes(
        AbstractObjectType* parent,
        int indexInParent,
        const std::vector<AbstractObjectType*>& children) const
    {
        auto& data = objectTypeCache[(quintptr) parent];
        data.type = parent;
        data.indexInParent = indexInParent;
        data.derivedTypeIds.clear();

        for (const auto& child: children)
        {
            if (!child->isReachable()
                || (liveTypesExcluded && (child->isLiveOnly() || child->isNonIndexable()))
                || (engine && !manager->isRelevantForEngine(child, engine)))
            {
                continue;
            }

            const int indexInParent = data.derivedTypeIds.size();
            recursivelyAddDetectableTypes(child, indexInParent, child->derivedTypes());
            data.derivedTypeIds.push_back((quintptr) child);
        }
    }

private:
    mutable QHash<quintptr, TypeData> objectTypeCache;
};

DetectableObjectTypeModel::DetectableObjectTypeModel(
    TaxonomyManager* taxonomyManager,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, taxonomyManager))
{
}

DetectableObjectTypeModel::~DetectableObjectTypeModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

QModelIndex DetectableObjectTypeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    const auto& parentData = d->data(parent.internalId());
    return createIndex(row, column, parentData.derivedTypeIds[row]);
}

QModelIndex DetectableObjectTypeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index, CheckIndexOption::DoNotUseParent)))
        return {};

    const auto& data = d->data(index.internalId());
    const auto baseTypeId = NX_ASSERT(data.type) ? (quintptr) data.type->base() : 0;
    if (!baseTypeId)
        return {};

    const auto baseData = d->data(baseTypeId);
    return createIndex(baseData.indexInParent, 0, baseTypeId);
}

int DetectableObjectTypeModel::rowCount(const QModelIndex& parent) const
{
    return NX_ASSERT(checkIndex(parent))
        ? d->data(parent.internalId()).derivedTypeIds.size()
        : 0;
}

int DetectableObjectTypeModel::columnCount(const QModelIndex& parent) const
{
    return NX_ASSERT(checkIndex(parent)) ? 1 : 0;
}

QVariant DetectableObjectTypeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index)))
        return {};

    const auto type = d->data(index.internalId()).type;
    if (!type)
        return {};

    switch (role)
    {
        case NameRole:
            return type->name();

        case IdRole:
            return type->id();

        default:
            return {};
    }
}

bool DetectableObjectTypeModel::liveTypesExcluded() const
{
    return d->liveTypesExcluded;
}

void DetectableObjectTypeModel::setLiveTypesExcluded(bool value)
{
    if (d->liveTypesExcluded == value)
        return;

    const ScopedReset reset(this);
    d->invalidateCache();
    d->liveTypesExcluded = value;
}

AbstractEngine* DetectableObjectTypeModel::engine() const
{
    return d->engine;
}

void DetectableObjectTypeModel::setEngine(AbstractEngine* value)
{
    if (d->engine == value)
        return;

    const ScopedReset reset(this);
    d->invalidateCache();
    d->engine = value;
}

} // namespace nx::vms::client::desktop
