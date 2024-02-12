// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/api/data/storage_flags.h>
#include <nx/vms/api/data/storage_space_data.h>

namespace nx::vms::api {

TEST(StorageSpaceData, Conversions)
{
    const auto assertEq =
        [](const auto& v3Data, const auto& v1Data)
        {
            ASSERT_EQ(v3Data.isUsedForWriting, v1Data.isUsedForWriting);
            ASSERT_EQ(v3Data.storageId, v1Data.storageId);
            ASSERT_EQ(v3Data.freeSpace, v1Data.freeSpace);
            ASSERT_EQ(v3Data.storageType, v1Data.storageType);
            ASSERT_EQ(v3Data.totalSpace, v1Data.totalSpace);
            ASSERT_EQ(v3Data.isBackup, v1Data.isBackup);
            ASSERT_EQ(v3Data.isExternal, v1Data.isExternal);
            ASSERT_EQ(v3Data.isOnline, v1Data.isOnline);
            ASSERT_EQ(v3Data.isWritable, v1Data.isWritable);
            ASSERT_EQ(v3Data.reservedSpace, v1Data.reservedSpace);
            ASSERT_EQ(
                (StorageStatus::beingChecked
                    | StorageStatus::used
                    | StorageStatus::dbReady
                    | StorageStatus::system),
                v1Data.storageStatus);
        };

    StorageSpaceDataV3 v3Data;
    v3Data.runtimeFlags = StorageRuntimeFlag::beingChecked | StorageRuntimeFlag::used;
    v3Data.persistentFlags = StoragePersistentFlag::dbReady | StoragePersistentFlag::system;
    v3Data.isUsedForWriting = true;
    v3Data.storageId = nx::Uuid::createUuid();
    v3Data.freeSpace = 42;
    v3Data.storageType = "some_type";
    v3Data.totalSpace = 43;
    v3Data.isBackup = true;
    v3Data.isExternal = true;
    v3Data.isOnline = true;
    v3Data.isWritable = true;
    v3Data.reservedSpace = 44;

    const auto v1Data = v3Data.toV1();
    assertEq(v3Data, v1Data);

    const auto v3List = std::vector<StorageSpaceData>{v3Data};
    const auto v1List = StorageSpaceData::toV1List(v3List);
    ASSERT_EQ(v3List.size(), v1List.size());
    for (size_t i = 0; i < v3List.size(); ++i)
        assertEq(v3List[i], v1List[i]);
}

TEST(StorageSpaceDataWithDbInfo, Conversions)
{
    const auto assertEq =
        [](const auto& v3Data, const auto& v1Data)
        {
            ASSERT_EQ(v3Data.isUsedForWriting, v1Data.isUsedForWriting);
            ASSERT_EQ(v3Data.storageId, v1Data.storageId);
            ASSERT_EQ(v3Data.freeSpace, v1Data.freeSpace);
            ASSERT_EQ(v3Data.storageType, v1Data.storageType);
            ASSERT_EQ(v3Data.totalSpace, v1Data.totalSpace);
            ASSERT_EQ(v3Data.isBackup, v1Data.isBackup);
            ASSERT_EQ(v3Data.isExternal, v1Data.isExternal);
            ASSERT_EQ(v3Data.isOnline, v1Data.isOnline);
            ASSERT_EQ(v3Data.isWritable, v1Data.isWritable);
            ASSERT_EQ(v3Data.reservedSpace, v1Data.reservedSpace);
            ASSERT_EQ(v3Data.serverId, v1Data.serverId);
            ASSERT_EQ(v3Data.name, v1Data.name);
            ASSERT_EQ(
                (StorageStatus::beingChecked
                    | StorageStatus::used
                    | StorageStatus::dbReady
                    | StorageStatus::system),
                v1Data.storageStatus);
        };

    StorageSpaceDataWithDbInfoV3 v3Data;
    v3Data.runtimeFlags = StorageRuntimeFlag::beingChecked | StorageRuntimeFlag::used;
    v3Data.persistentFlags = StoragePersistentFlag::dbReady | StoragePersistentFlag::system;
    v3Data.isUsedForWriting = true;
    v3Data.storageId = nx::Uuid::createUuid();
    v3Data.freeSpace = 42;
    v3Data.storageType = "some_type";
    v3Data.totalSpace = 43;
    v3Data.isBackup = true;
    v3Data.isExternal = true;
    v3Data.isOnline = true;
    v3Data.isWritable = true;
    v3Data.reservedSpace = 44;
    v3Data.serverId = nx::Uuid::createUuid();
    v3Data.name = "name";

    const auto v1Data = v3Data.toV1();
    assertEq(v3Data, v1Data);

    const auto v3List = std::vector<StorageSpaceDataWithDbInfo>{v3Data};
    const auto v1List = StorageSpaceDataWithDbInfo::toV1List(v3List);
    ASSERT_EQ(v3List.size(), v1List.size());
    for (size_t i = 0; i < v3List.size(); ++i)
        assertEq(v3List[i], v1List[i]);
}

} // namespace nx::vms::api
