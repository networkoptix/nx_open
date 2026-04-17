// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/mobile/models/resource_tree_search_model.h>
#include <nx/vms/client/mobile/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/mobile/test_support/test_context.h>
#include <nx/vms/common/test_support/model/item_model_test_helper.h>

namespace nx::vms::client::core { class AccessController; }

namespace nx::vms::client::mobile {
namespace test {

class ResourceTreeModelTest: public ApplicationContextBasedTest,
    public nx::vms::common::test::ItemModelTestHelper
{
    using base_type = ApplicationContextBasedTest;

protected:
    using NodeType = nx::vms::client::core::ResourceTree::NodeType;
    using AccessController = nx::vms::client::core::AccessController;

    virtual void SetUp() override;
    virtual void TearDown() override;

    AccessController* accessController() const;

    QnUserResourcePtr loginAs(const QString& name,
        const std::vector<nx::Uuid>& groupIds = {});
    QnUserResourcePtr loginAsAdministrator(const QString& name);
    QnUserResourcePtr loginAsPowerUser(const QString& name);
    QnUserResourcePtr loginAsLiveViewer(const QString& name);
    QnUserResourcePtr loginAsAdvancedViewer(const QString& name);
    QnUserResourcePtr loginAsCustomUser(const QString& name);
    QnUserResourcePtr currentUser() const;
    void logout();

    QAbstractItemModel* model() const override;
    void setSearchString(const QString& searchString);

protected:
    std::unique_ptr<core::entity_item_model::EntityItemModel> m_treeModel;
    std::unique_ptr<ResourceTreeSearchModel> m_searchModel;
    std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> m_treeBuilder;
    std::unique_ptr<core::entity_item_model::AbstractEntity> m_rootEntity;
};

} // namespace test
} // namespace nx::vms::client::mobile
