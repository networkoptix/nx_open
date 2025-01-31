// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/storage_space_data.h>
#include <nx/vms/common/resource/camera_resource_stub.h>
#include <nx/vms/common/system_context.h>
#include <transaction/transaction_descriptor.h>

#include <gtest/gtest.h>

namespace ec2::transaction_descriptor::test {

using namespace nx::vms::common;

class CanModifyStorageTest: public ::testing::Test
{
protected:
    SystemContext context;
    CanModifyStorageData data;
    nx::vms::api::ResourceData existingResource;
    nx::Uuid parentId = nx::Uuid::createUuid();
    bool logFuncCalled = false;
    bool getExistingStorageCalled = false;

    CanModifyStorageTest():
        ::testing::Test(),
        context(SystemContext::Mode::server, nx::Uuid::createUuid())
    {
        existingResource.parentId = parentId;
        existingResource.url = "some://url";
        data.getExistingStorageDataFunc =
            [this]()
            {
                getExistingStorageCalled = true;
                return existingResource;
            };
        data.logErrorFunc = [this](const QString&) { logFuncCalled = true; };
    }
};

TEST_F(CanModifyStorageTest, GeneralAccessCheckFailed)
{
    data.modifyResourceResult = ErrorCode::forbidden;
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(&context, data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_FALSE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, NoExistingResource)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    ASSERT_EQ(ErrorCode::ok, canModifyStorage(&context, data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_FALSE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, ResourceExists_EqualParentIds_EqualUrls)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = true;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    ASSERT_EQ(ErrorCode::ok, canModifyStorage(&context, data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_TRUE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, ResourceExists_EqualParentIds_DifferentUrls)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = true;
    data.request.parentId = existingResource.parentId;
    data.request.url = "some://other/url";
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(&context, data));
    ASSERT_TRUE(logFuncCalled);
    ASSERT_TRUE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, ForbiddenMainCloudStorage)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    data.request.storageType = nx::vms::api::kCloudStorageType;
    data.request.isBackup = false;
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(&context, data));
}

TEST_F(CanModifyStorageTest, AllowedBackupCloudStorage)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    data.request.storageType = nx::vms::api::kCloudStorageType;
    data.request.isBackup = true;
    ASSERT_EQ(ErrorCode::ok, canModifyStorage(&context, data));
}

TEST_F(CanModifyStorageTest, usedForWritingForbidenIfNoLicense)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    data.request.storageType = nx::vms::api::kCloudStorageType;
    data.request.isBackup = true;
    data.request.usedForWriting = true;

    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    camera->setBackupPolicy(nx::vms::api::BackupPolicy::on);
    context.resourcePool()->addResource(camera);
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(&context, data));
    context.resourcePool()->removeResource(camera);
}

TEST_F(CanModifyStorageTest, SetUsedForWritingForbiddenForLocalWhenCloudIsActive)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    data.hasUsedCloudStorage = true;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    data.request.storageType = "local";
    data.request.isBackup = true;
    data.request.usedForWriting = true;
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(&context, data));
}

} // namespace ec2::transaction_descriptor::test
