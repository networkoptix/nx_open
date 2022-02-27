// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detectable_object_type_model.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics::taxonomy;

class DetectableObjectTypeModel::Private
{
    DetectableObjectTypeModel* const q;
    QPointer<AbstractStateWatcher> watcher;

public:
    Private(DetectableObjectTypeModel* q, AbstractStateWatcher* watcher):
        q(q),
        watcher(watcher)
    {
        if (!NX_ASSERT(watcher))
            return;

        QObject::connect(watcher, &AbstractStateWatcher::stateChanged, q,
            [this]()
            {
                const ScopedReset reset(this->q);
                objectTypeCache.clear();
                currentTaxonomy.reset();
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

private:
    void ensureObjectTypeCache() const
    {
        if (!objectTypeCache.empty() || !NX_ASSERT(watcher))
            return;

        currentTaxonomy = watcher->state(); //< Preserve shared pointer.
        if (!currentTaxonomy)
            return;

        recursivelyAddDetectableTypes(objectTypeCache, /*parent*/ nullptr, /*indexInParent*/ -1,
            currentTaxonomy->rootObjectTypes());
    }

    using ObjectTypeCache = QHash<quintptr, TypeData>;

    static void recursivelyAddDetectableTypes(
        ObjectTypeCache& cache,
        AbstractObjectType* parent,
        int indexInParent,
        const std::vector<AbstractObjectType*>& children)
    {
        auto& data = cache[(quintptr) parent];
        data.type = parent;
        data.indexInParent = indexInParent;
        data.derivedTypeIds.clear();

        for (const auto& child: children)
        {
            if (child->isPrivate())
                continue;

            const int indexInParent = data.derivedTypeIds.size();
            recursivelyAddDetectableTypes(cache, child, indexInParent, child->derivedTypes());
            data.derivedTypeIds.push_back((quintptr) child);
        }
    }

private:
    mutable ObjectTypeCache objectTypeCache;
    mutable std::shared_ptr<AbstractState> currentTaxonomy;
};

DetectableObjectTypeModel::DetectableObjectTypeModel(
    AbstractStateWatcher* taxonomyWatcher,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, taxonomyWatcher))
{
}

DetectableObjectTypeModel::~DetectableObjectTypeModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

QModelIndex DetectableObjectTypeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0 || row >= rowCount(parent))
        return {};

    const auto& parentData = d->data((quintptr) parent.internalId());
    return createIndex(row, column, parentData.derivedTypeIds[row]);
}

QModelIndex DetectableObjectTypeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index, CheckIndexOption::DoNotUseParent)))
        return {};

    const auto& data = d->data(index.internalId());
    return data.type
        ? createIndex(data.indexInParent, 0, (quintptr) data.type->base())
        : QModelIndex();
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

} // namespace nx::vms::client::desktop
