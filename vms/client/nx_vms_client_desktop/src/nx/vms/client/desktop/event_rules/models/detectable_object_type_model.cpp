// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "detectable_object_type_model.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/analytics/taxonomy/object_type.h>

namespace nx::vms::client::desktop {

using namespace nx::analytics::taxonomy;
using namespace nx::vms::client::core::analytics::taxonomy;

class DetectableObjectTypeModel::Private
{
public:
    struct TypeData
    {
        ObjectType* parent = nullptr;
        int indexInParent = -1;
    };

    Private(DetectableObjectTypeModel* q, core::analytics::taxonomy::AnalyticsFilterModel* filterModel):
        q(q),
        filterModel(filterModel)
    {
        if (!NX_ASSERT(filterModel))
            return;

        QObject::connect(filterModel, &AnalyticsFilterModel::objectTypesChanged, q,
            [this]()
            {
                ScopedReset reset(this->q);
                updateData();
            });

        updateData();
    }

    void updateData(const std::vector<ObjectType*>& types)
    {
        for (int i = 0; i < types.size(); ++i)
        {
            ObjectType* type = types[i];
            data.insert(type, TypeData{.indexInParent = i});
            updateData(type->derivedObjectTypes());
        }
    }

    void updateData()
    {
        data.clear();
        updateData(filterModel->objectTypes());
    }

    ObjectType* type(const QModelIndex& index) const
    {
        return static_cast<ObjectType*>(index.internalPointer());
    }

public:
    DetectableObjectTypeModel* const q;
    AnalyticsFilterModel* const filterModel;
    QMap<ObjectType*, TypeData> data;
};

DetectableObjectTypeModel::DetectableObjectTypeModel(
    core::analytics::taxonomy::AnalyticsFilterModel* filterModel,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, filterModel))
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

    ObjectType* type = d->type(parent);
    return createIndex(
        row, column, type ? type->derivedObjectTypes()[row] : d->filterModel->objectTypes()[row]);
}

QModelIndex DetectableObjectTypeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index, CheckIndexOption::DoNotUseParent)))
        return {};

    ObjectType* type = d->type(index);
    ObjectType* parent = type ? type->baseObjectType() : nullptr;
    if (!parent)
        return {};

    return parent
        ? createIndex(d->data[parent].indexInParent, 0, parent)
        : QModelIndex();
}

int DetectableObjectTypeModel::rowCount(const QModelIndex& parent) const
{
    if (!NX_ASSERT(checkIndex(parent)))
        return 0;

    return parent.isValid()
        ? d->type(parent)->derivedObjectTypes().size()
        : d->filterModel->objectTypes().size(); //< Parent is the root item.
}

int DetectableObjectTypeModel::columnCount(const QModelIndex& parent) const
{
    return NX_ASSERT(checkIndex(parent)) ? 1 : 0;
}

QVariant DetectableObjectTypeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !NX_ASSERT(checkIndex(index)))
        return {};

    ObjectType* type = d->type(index);
    if (!type)
        return {};

    switch (role)
    {
        case NameRole:
            return type->name();

        case IdsRole:
            return d->filterModel->getAnalyticsObjectTypeIds(type);

        case MainIdRole:
            return type->mainTypeId();

        default:
            return {};
    }
}

void DetectableObjectTypeModel::setLiveTypesExcluded(bool value)
{
    d->filterModel->setLiveTypesExcluded(value);
}

void DetectableObjectTypeModel::setEngine(AbstractEngine* engine)
{
    d->filterModel->setSelectedEngine(engine);
}

AnalyticsFilterModel* DetectableObjectTypeModel::sourceModel() const
{
    return d->filterModel;
}

} // namespace nx::vms::client::desktop
