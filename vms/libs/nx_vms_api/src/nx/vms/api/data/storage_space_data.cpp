// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_space_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/storage_flags.h>

namespace nx::vms::api {

namespace {

void updateRuntimeFlagsToV1(
    nx::vms::api::StorageRuntimeFlags runtimeFlags, nx::vms::api::StorageStatuses* statusFlags)
{
    if (runtimeFlags.testFlag(nx::vms::api::StorageRuntimeFlag::beingChecked))
        *statusFlags |= nx::vms::api::StorageStatus::beingChecked;
    if (runtimeFlags.testFlag(nx::vms::api::StorageRuntimeFlag::beingRebuilt))
        *statusFlags |= nx::vms::api::StorageStatus::beingRebuilt;
    if (runtimeFlags.testFlag(nx::vms::api::StorageRuntimeFlag::disabled))
        *statusFlags |= nx::vms::api::StorageStatus::disabled;
    if (runtimeFlags.testFlag(nx::vms::api::StorageRuntimeFlag::tooSmall))
        *statusFlags |= nx::vms::api::StorageStatus::tooSmall;
    if (runtimeFlags.testFlag(nx::vms::api::StorageRuntimeFlag::used))
        *statusFlags |= nx::vms::api::StorageStatus::used;
}

void updatePersistentFlagsToV1(
    nx::vms::api::StoragePersistentFlags runtimeFlags, nx::vms::api::StorageStatuses* statusFlags)
{
    if (runtimeFlags.testFlag(nx::vms::api::StoragePersistentFlag::dbReady))
        *statusFlags |= nx::vms::api::StorageStatus::dbReady;
    if (runtimeFlags.testFlag(nx::vms::api::StoragePersistentFlag::removable))
        *statusFlags |= nx::vms::api::StorageStatus::removable;
    if (runtimeFlags.testFlag(nx::vms::api::StoragePersistentFlag::system))
        *statusFlags |= nx::vms::api::StorageStatus::system;
}

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceDataBase, (ubjson)(json), StorageSpaceDataBase_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceDataV1, (ubjson)(json), StorageSpaceDataV1_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceDataV3, (ubjson)(json), StorageSpaceDataV3_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceDataWithDbInfoV1, (ubjson)(json), StorageSpaceDataWithDbInfoV1_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageSpaceDataWithDbInfoV3, (ubjson)(json), StorageSpaceDataWithDbInfoV3_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageCheckFilter, (json), StorageCheckFilter_Fields)

StorageSpaceDataV1 StorageSpaceDataV3::toV1() const
{
    StorageSpaceDataV1 result(static_cast<const StorageSpaceDataBase&>(*this));
    updatePersistentFlagsToV1(this->persistentFlags, &result.storageStatus);
    updateRuntimeFlagsToV1(this->runtimeFlags, &result.storageStatus);
    return result;
}

StorageSpaceDataListV1 StorageSpaceDataV3::toV1List(const std::vector<StorageSpaceDataV3>& v3List)
{
    StorageSpaceDataListV1 result;
    for (const auto& v3Data: v3List)
        result.push_back(v3Data.toV1());

    return result;
}

StorageSpaceDataWithDbInfoV1 StorageSpaceDataWithDbInfoV3::toV1() const
{
    StorageSpaceDataWithDbInfoV1 result(static_cast<const StorageSpaceDataBase&>(*this));
    result.serverId = this->serverId;
    result.name = this->name;
    updatePersistentFlagsToV1(this->persistentFlags, &result.storageStatus);
    updateRuntimeFlagsToV1(this->runtimeFlags, &result.storageStatus);
    return result;
}

StorageSpaceDataWithDbInfoListV1 StorageSpaceDataWithDbInfoV3::toV1List(
    const std::vector<StorageSpaceDataWithDbInfoV3>& v3List)
{
    StorageSpaceDataWithDbInfoListV1 result;
    for (const auto& v3Data: v3List)
        result.push_back(v3Data.toV1());

    return result;
}

} // namespace nx::vms::api
