// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/system_finder/private/cloud_system_description.h>
#include <nx/vms/client/core/system_finder/private/local_system_description.h>
#include <ui/models/systems_model.h>

#include "test_systems_controller.h"

namespace nx::vms::client::core {
namespace test {

namespace {

using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

SystemDescriptionPtr createSystemsDescription()
{
    return LocalSystemDescription::create(
        /*id*/ nx::Uuid::createUuid().toSimpleString(),
        /*localSystemId*/ nx::Uuid::createUuid(),
        /*cloudSystemId*/ QString(),
        "name");
}

// All necessary models.
class SystemsModelTest: public testing::Test
{
protected:
    SystemsModelTest()
    {
        controller.reset(new TestSystemsController());
        model.reset(new QnSystemsModel(controller.get()));
    }

    void ifSystemFound()
    {
        ASSERT_TRUE(controller->discoverSystem(createSystemsDescription()));
    }

    void ifSystemLost()
    {
        ASSERT_FALSE(controller->systemsList().empty());
        ASSERT_TRUE(controller->loseSystem(controller->systemsList().first()->id()));
    }

    void ifSystemLost(const QString& systemId)
    {
        ASSERT_TRUE(controller->loseSystem(systemId));
    }

    void thenNumberOfSystemsInModel(int number)
    {
        ASSERT_EQ(number, model->rowCount());
    }

    void givenLocalSystem(const nx::Uuid& localId)
    {
        ASSERT_TRUE(controller->discoverSystem(LocalSystemDescription::create(
            localId.toSimpleString(), localId, /*cloudSystemId*/ QString(), "Local Site")));
    }

    void givenCloudSystem(const QString& cloudId, const nx::Uuid& localId)
    {
        ASSERT_TRUE(controller->discoverSystem(QnCloudSystemDescription::create(
            {.cloudId = cloudId, .localId = localId, .name = "Cloud Site", .online = false})));
    }

    void thenSystemInRow(int row, const QString& systemId, bool isCloudSystem)
    {
        const auto index = model->index(row, 0);
        EXPECT_EQ(systemId, index.data(QnSystemsModel::SystemIdRoleId).toString());
        EXPECT_EQ(isCloudSystem, index.data(QnSystemsModel::IsCloudSystemRoleId).toBool());
    }

    void setVisibilityScope(TileVisibilityScope scope)
    {
        model->setData(
            model->index(0, 0), QVariant::fromValue(scope), QnSystemsModel::VisibilityScopeRoleId);
    }

    void thenSystemVisibilityScope(TileVisibilityScope scope)
    {
        ASSERT_EQ(scope,
            controller->visibilityScope(controller->systemsList().first()->localId()));
    }

    std::unique_ptr<TestSystemsController> controller;
    std::unique_ptr<QnSystemsModel> model;
};

} // namespace

TEST_F(SystemsModelTest, emptyModelHasNoRows)
{
    thenNumberOfSystemsInModel(0);
}

TEST_F(SystemsModelTest, systemsDiscoverAndLost)
{
    ifSystemFound();
    ifSystemFound();
    thenNumberOfSystemsInModel(2);
    ifSystemLost();
    thenNumberOfSystemsInModel(1);
}

// SystemsFinder intentionally allows several systems with the same local id, so cloning of the
// same system is allowed, and its clones can co-exist as one local and several cloud systems in
// different cloud instances.
// So the SystemsModel may contain several rows with the same local id.
// Losing one of these systems must not remove a row of another one.
TEST_F(SystemsModelTest, sameLocalId_L_then_C__losingCloudSystemKeepsLocalSystem)
{
    const auto localId = nx::Uuid::createUuid();
    const auto cloudId = nx::Uuid::createUuid().toSimpleString();

    givenLocalSystem(localId);
    givenCloudSystem(cloudId, localId);
    thenNumberOfSystemsInModel(2);

    ifSystemLost(cloudId);

    thenNumberOfSystemsInModel(1);
    thenSystemInRow(0, localId.toSimpleString(), /*isCloudSystem*/ false);
}

// The same as above, but the systems are discovered in the reverse order.
TEST_F(SystemsModelTest, sameLocalId_C_then_L__losingCloudSystemKeepsLocalSystem)
{
    const auto localId = nx::Uuid::createUuid();
    const auto cloudId = nx::Uuid::createUuid().toSimpleString();

    givenCloudSystem(cloudId, localId);
    givenLocalSystem(localId);
    thenNumberOfSystemsInModel(2);

    ifSystemLost(cloudId);

    thenNumberOfSystemsInModel(1);
    thenSystemInRow(0, localId.toSimpleString(), /*isCloudSystem*/ false);
}

TEST_F(SystemsModelTest, sameLocalId_L_then_C__losingLocalSystemKeepsCloudSystem)
{
    const auto localId = nx::Uuid::createUuid();
    const auto cloudId = nx::Uuid::createUuid().toSimpleString();

    givenLocalSystem(localId);
    givenCloudSystem(cloudId, localId);
    thenNumberOfSystemsInModel(2);

    ifSystemLost(localId.toSimpleString());

    thenNumberOfSystemsInModel(1);
    thenSystemInRow(0, cloudId, /*isCloudSystem*/ true);
}

TEST_F(SystemsModelTest, sameLocalId_C_then_L__losingLocalSystemKeepsCloudSystem)
{
    const auto localId = nx::Uuid::createUuid();
    const auto cloudId = nx::Uuid::createUuid().toSimpleString();

    givenCloudSystem(cloudId, localId);
    givenLocalSystem(localId);
    thenNumberOfSystemsInModel(2);

    ifSystemLost(localId.toSimpleString());

    thenNumberOfSystemsInModel(1);
    thenSystemInRow(0, cloudId, /*isCloudSystem*/ true);
}

TEST_F(SystemsModelTest, rowIndexesAfterRemoval)
{
    const auto first = nx::Uuid::createUuid();
    const auto second = nx::Uuid::createUuid();
    const auto third = nx::Uuid::createUuid();

    givenLocalSystem(first);
    givenLocalSystem(second);
    givenLocalSystem(third);
    thenNumberOfSystemsInModel(3);

    ifSystemLost(second.toSimpleString());

    thenNumberOfSystemsInModel(2);
    EXPECT_EQ(model->getRowIndex(first.toSimpleString()), 0);
    EXPECT_EQ(model->getRowIndex(second.toSimpleString()), -1);
    EXPECT_EQ(model->getRowIndex(third.toSimpleString()), 1);
}

TEST_F(SystemsModelTest, controllerSetScopeInfo)
{
    ifSystemFound();
    thenSystemVisibilityScope(TileVisibilityScope::DefaultTileVisibilityScope);
    setVisibilityScope(TileVisibilityScope::FavoriteTileVisibilityScope);
    thenSystemVisibilityScope(TileVisibilityScope::FavoriteTileVisibilityScope);
    setVisibilityScope(TileVisibilityScope::HiddenTileVisibilityScope);
    thenSystemVisibilityScope(TileVisibilityScope::HiddenTileVisibilityScope);
}

} // namespace test
} // namespace nx::vms::client::core
