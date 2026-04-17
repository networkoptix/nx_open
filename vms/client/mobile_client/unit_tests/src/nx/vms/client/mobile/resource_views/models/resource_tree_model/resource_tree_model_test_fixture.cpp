// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/test_utils/resource_tree_model_index_condition.h>
#include <nx/vms/client/mobile/system_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::mobile {
namespace test {

using namespace nx::vms::api;
using namespace core::test::index_condition;
using namespace nx::vms::client::core;

void ResourceTreeModelTest::SetUp()
{
    base_type::SetUp();
    m_treeBuilder =
        std::make_unique<entity_resource_tree::ResourceTreeEntityBuilder>(systemContext());
    m_treeModel.reset(new entity_item_model::EntityItemModel());
    m_searchModel.reset(new ResourceTreeSearchModel());
    m_searchModel->setSourceModel(m_treeModel.get());
}

void ResourceTreeModelTest::TearDown()
{
    logout();
    m_searchModel.reset();
    m_treeModel.reset();
    m_treeBuilder.reset();
    base_type::TearDown();
}

ResourceTreeModelTest::AccessController* ResourceTreeModelTest::accessController() const
{
    return systemContext()->accessController();
}

QnUserResourcePtr ResourceTreeModelTest::loginAs(
    const QString& name, const std::vector<nx::Uuid>& groupIds)
{
    logout();
    const auto users = resourcePool()->getResources<QnUserResource>();

    const auto itr = std::ranges::find_if(users,
        [&name, &groupIds](const QnUserResourcePtr& user)
        {
            return user->getName() == name && user->allGroupIds() == groupIds;
        });

    QnUserResourcePtr user;
    if (itr != users.cend())
        user = *itr;
    else
        user = addUser(name, groupIds);

    accessController()->setUser(user);

    m_treeBuilder->setUser(user);
    m_treeModel->setRootEntity(nullptr);
    m_rootEntity = m_treeBuilder->createTreeEntity();
    m_treeModel->setRootEntity(m_rootEntity.get());

    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdministrator(const QString& name)
{
    return loginAs(name, {api::kAdministratorsGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsPowerUser(const QString& name)
{
    return loginAs(name, {api::kPowerUsersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name)
{
    return loginAs(name, {api::kLiveViewersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdvancedViewer(const QString& name)
{
    return loginAs(name, {api::kAdvancedViewersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsCustomUser(const QString& name)
{
    return loginAs(name);
}

QnUserResourcePtr ResourceTreeModelTest::currentUser() const
{
    return accessController()->user();
}

void ResourceTreeModelTest::logout()
{
    accessController()->setUser(QnUserResourcePtr());
    m_treeBuilder->setUser(QnUserResourcePtr());
    m_treeModel->setRootEntity(nullptr);
    m_rootEntity = m_treeBuilder->createTreeEntity();
    m_treeModel->setRootEntity(m_rootEntity.get());
}

QAbstractItemModel* ResourceTreeModelTest::model() const
{
    return m_searchModel.get();
}

void ResourceTreeModelTest::setSearchString(const QString& searchString)
{
    m_searchModel->setFilterRegularExpression(searchString);
}

} // namespace test
} // namespace nx::vms::client::mobile
