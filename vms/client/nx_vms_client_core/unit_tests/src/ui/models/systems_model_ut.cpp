// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <network/local_system_description.h>
#include <ui/models/systems_model.h>

#include "test_systems_controller.h"

namespace nx::vms::client::core {
namespace test {

namespace {

using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

QnSystemDescriptionPtr createSystemsDescription()
{
    return QnLocalSystemDescription::create(
        nx::Uuid::createUuid().toSimpleString(),
        nx::Uuid::createUuid(),
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
        controller->emitSystemDiscovered(createSystemsDescription());
    }

    void ifSystemLost()
    {
        controller->emitSystemLost(controller->systemsList().first()->id());
    }

    void thenNumberOfSystemsInModel(int number)
    {
        ASSERT_EQ(number, model->rowCount());
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
