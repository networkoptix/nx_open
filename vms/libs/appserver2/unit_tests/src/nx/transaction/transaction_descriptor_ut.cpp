// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction/transaction_descriptor.h"

#include <gtest/gtest.h>

namespace ec2::transaction_descriptor::test {

class CanModifyStorageTest: public ::testing::Test
{
protected:
    CanModifyStorageData data;
    nx::vms::api::ResourceData existingResource;
    QnUuid parentId = QnUuid::createUuid();
    bool logFuncCalled = false;
    bool getExistingStorageCalled = false;

    CanModifyStorageTest()
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
    ASSERT_EQ(ErrorCode::forbidden, canModifyStorage(data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_FALSE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, NoExistingResource)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = false;
    ASSERT_EQ(ErrorCode::ok, canModifyStorage(data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_FALSE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, ResourceExists_EqualParentIds_EqualUrls)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = true;
    data.request.parentId = existingResource.parentId;
    data.request.url = existingResource.url;
    ASSERT_EQ(ErrorCode::ok, canModifyStorage(data));
    ASSERT_FALSE(logFuncCalled);
    ASSERT_TRUE(getExistingStorageCalled);
}

TEST_F(CanModifyStorageTest, ResourceExists_EqualParentIds_DifferentUrls)
{
    data.modifyResourceResult = ErrorCode::ok;
    data.hasExistingStorage = true;
    data.request.parentId = existingResource.parentId;
    data.request.url = "some://other/url";
    ASSERT_EQ(ErrorCode::badRequest, canModifyStorage(data));
    ASSERT_TRUE(logFuncCalled);
    ASSERT_TRUE(getExistingStorageCalled);
}

} // namespace ec2::transaction_descriptor::test
