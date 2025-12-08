// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/generic_item/generic_item.h>
#include <nx/vms/client/core/resource_views/entity_item_model/item/shared_item/shared_item.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/recorder_item_data_helper.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

using entity_item_model::GenericItem;
using entity_item_model::InvalidatorPtr;

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider resourceNameProvider(
    const QnResourcePtr& resource);
NX_VMS_CLIENT_CORE_API InvalidatorPtr resourceNameInvalidator(const QnResourcePtr & resource);

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider recorderNameProvider(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper);
NX_VMS_CLIENT_CORE_API InvalidatorPtr recorderNameInvalidator(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper);

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider recorderIconProvider(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper);
NX_VMS_CLIENT_CORE_API InvalidatorPtr recorderIconInvalidator(
    const QString& groupId,
    const QSharedPointer<RecorderItemDataHelper>& recorderItemDataHelper);

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider cloudLayoutIconProvider(
    const QnLayoutResourcePtr& layout);
NX_VMS_CLIENT_CORE_API InvalidatorPtr cloudLayoutIconInvalidator(
    const QnLayoutResourcePtr& layout);

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider cloudLayoutExtraStatusProvider(
    const QnLayoutResourcePtr& layout);
NX_VMS_CLIENT_CORE_API InvalidatorPtr cloudLayoutExtraStatusInvalidator(
    const QnLayoutResourcePtr& layout);

NX_VMS_CLIENT_CORE_API GenericItem::DataProvider cloudLayoutGroupIdProvider(
    const QnLayoutResourcePtr& layout);
NX_VMS_CLIENT_CORE_API InvalidatorPtr cloudLayoutGroupIdInvalidator(
    const QnLayoutResourcePtr& layout);

} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
