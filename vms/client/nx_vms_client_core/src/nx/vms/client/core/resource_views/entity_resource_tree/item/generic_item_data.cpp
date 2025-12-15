// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_item_data.h"

#include <core/resource/layout_resource.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/core/skin/resource_icon_cache.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

using namespace nx::vms::client::core::entity_item_model;

GenericItem::DataProvider resourceNameProvider(const QnResourcePtr& resource)
{
    return [resource] { return resource->getName(); };
}

InvalidatorPtr resourceNameInvalidator(const QnResourcePtr& resource)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(resource->connect(
        resource.get(), &QnResource::nameChanged,
        [invalidator = result.get()] { invalidator->invalidate(); }));

    return result;
}

GenericItem::DataProvider recorderNameProvider(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    return [groupId, recorderItemDataHelper]
        {
            return recorderItemDataHelper->groupedDeviceName(groupId);
        };
}

InvalidatorPtr recorderNameInvalidator(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(recorderItemDataHelper->connect(
        recorderItemDataHelper.get(), &RecorderItemDataHelper::groupedDeviceNameChanged,
        [groupId, invalidator = result.get()](const QString& changedGroupId)
        {
            if (groupId == changedGroupId)
                invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider recorderIconProvider(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    return
        [groupId, recorderItemDataHelper]()
        {
            return recorderItemDataHelper->iconKey(groupId);
        };
}

InvalidatorPtr recorderIconInvalidator(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(recorderItemDataHelper->connect(
        recorderItemDataHelper.get(), &RecorderItemDataHelper::groupedDeviceStatusChanged,
        [groupId, invalidator = result.get()](const QString& changedGroupId)
        {
            if (groupId == changedGroupId)
                invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider cloudLayoutIconProvider(const QnLayoutResourcePtr& /*layout*/)
{
    return []() -> int { return ResourceIconCache::CloudLayout; };
}

entity_item_model::InvalidatorPtr cloudLayoutIconInvalidator(const QnLayoutResourcePtr& layout)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        layout.get(),
        &QnLayoutResource::lockedChanged,
        [invalidator = result.get()]
        {
            invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider cloudLayoutExtraStatusProvider(const QnLayoutResourcePtr& layout)
{
    using namespace nx::vms::client::core;
    return
        [layout]
        {
            return layout->locked()
                ? static_cast<int>(ResourceExtraStatusFlag::locked)
                : static_cast<int>(ResourceExtraStatusFlag::empty);
        };
}

InvalidatorPtr cloudLayoutExtraStatusInvalidator(const QnLayoutResourcePtr& layout)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        layout.get(),
        &QnLayoutResource::lockedChanged,
        [invalidator = result.get()]
        {
            invalidator->invalidate();
        }));

    return result;
}

GenericItem::DataProvider cloudLayoutGroupIdProvider(const QnLayoutResourcePtr& layout)
{
    using namespace nx::vms::client::core::entity_resource_tree::resource_grouping;

    return
        [layout]
        {
            return resourceCustomGroupId(layout);
        };
}

InvalidatorPtr cloudLayoutGroupIdInvalidator(const QnLayoutResourcePtr& layout)
{
    auto result = std::make_shared<Invalidator>();

    result->connections()->add(QObject::connect(
        layout.get(),
        &QnLayoutResource::propertyChanged,
        [invalidator = result.get()](const QnResourcePtr&, const QString& key)
        {
            if (key == nx::vms::api::resource_properties::kCustomGroupIdPropertyKey)
                invalidator->invalidate();
        }));

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
