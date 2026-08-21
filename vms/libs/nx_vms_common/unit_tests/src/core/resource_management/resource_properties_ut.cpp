// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource_management/resource_properties.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::common::test {

namespace {

static const QString kKey("mediaStreams");

class ResourcePropertyDictionaryTest: public ::testing::Test
{
protected:
    SystemContext systemContext{SystemContext::Mode::unitTests, nx::Uuid::createUuid()};

    // There is no message bus, so saveParamsAsync() just drains the modified items, which is what
    // it does before sending the transaction.
    QnResourcePropertyDictionary dictionary{&systemContext};
    const nx::Uuid resourceId = nx::Uuid::createUuid();
};

} // namespace

// A notification about an already sent save must not drop the modification mark of a newer local
// value - otherwise that value is never saved. See VMS-62778.
TEST_F(ResourcePropertyDictionaryTest, notificationDoesNotDropUnsavedModification)
{
    dictionary.setValue(resourceId, kKey, "primary");
    dictionary.saveParamsAsync(resourceId);
    dictionary.setValue(resourceId, kKey, "primary+secondary");

    dictionary.setValue(resourceId, kKey, "primary", /*markDirty*/ false);

    // The notification is still applied, but the unsaved value survives to be sent.
    EXPECT_EQ(dictionary.value(resourceId, kKey), "primary");
    EXPECT_EQ(dictionary.modifiedProperties(resourceId)[kKey], "primary+secondary");
}

TEST_F(ResourcePropertyDictionaryTest, unsavedValueIsRestoredByItsOwnNotification)
{
    dictionary.setValue(resourceId, kKey, "primary");
    dictionary.saveParamsAsync(resourceId);
    dictionary.setValue(resourceId, kKey, "primary+secondary");
    dictionary.setValue(resourceId, kKey, "primary", /*markDirty*/ false);

    dictionary.saveParamsAsync(resourceId);
    dictionary.setValue(resourceId, kKey, "primary+secondary", /*markDirty*/ false);

    EXPECT_EQ(dictionary.value(resourceId, kKey), "primary+secondary");
    EXPECT_TRUE(dictionary.modifiedProperties(resourceId).empty());
}

TEST_F(ResourcePropertyDictionaryTest, notificationIsAppliedWhenNothingIsPending)
{
    dictionary.setValue(resourceId, kKey, "primary");
    dictionary.saveParamsAsync(resourceId);

    dictionary.setValue(resourceId, kKey, "primary+secondary", /*markDirty*/ false);

    EXPECT_EQ(dictionary.value(resourceId, kKey), "primary+secondary");
    EXPECT_TRUE(dictionary.modifiedProperties(resourceId).empty());
}

} // namespace nx::vms::common::test
