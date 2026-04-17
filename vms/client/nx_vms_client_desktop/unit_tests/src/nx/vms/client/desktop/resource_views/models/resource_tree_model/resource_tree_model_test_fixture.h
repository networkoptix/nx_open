// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/core/test_utils/resource_tree_model_index_condition.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_composer.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/test_support/model/item_model_test_helper.h>

class QnResourceAccessManager;
class QnRuntimeInfoManager;

namespace nx::vms::api { enum class WebPageSubtype; }
namespace nx::vms::client::core { class AccessController; }
namespace nx::vms::client::core::entity_item_model { class EntityItemModel; }

namespace nx::vms::client::desktop {

namespace entity_resource_tree { class ResourceTreeComposer; };

namespace test {

struct WithCrossSiteFeatures
{
    static ApplicationContext::Features features()
    {
        auto result = ApplicationContext::Features::none();
        result.core.base.flags.setFlag(common::ApplicationContext::FeatureFlag::networking);
        result.core.flags.setFlag(core::ApplicationContext::FeatureFlag::cross_site);
        return result;
    }
};

class ResourceTreeModelTest:
    public AppContextBasedTest<WithCrossSiteFeatures>,
    public ::testing::WithParamInterface<nx::vms::api::WebPageSubtype>,
    public common::test::ItemModelTestHelper
{
protected:
    using NodeType = nx::vms::client::core::ResourceTree::NodeType;
    using WebPageSubtype = nx::vms::api::WebPageSubtype;
    using AccessController = nx::vms::client::core::AccessController;

    virtual void SetUp() override;
    virtual void TearDown() override;

    AccessController* accessController() const;

    void addOtherServer(const QString& name) const;
    QnAviResourcePtr addLocalMedia(const QString& path) const;
    QnFileLayoutResourcePtr addFileLayout(const QString& path,
        bool isEncrypted = false) const;

    QnWebPageResourcePtr addWebPage(const QString& name, WebPageSubtype subtype) const;
    QnWebPageResourcePtr addProxiedWebPage(
        const QString& name,
        WebPageSubtype subtype,
        const nx::Uuid& serverId) const;

    QnVideoWallResourcePtr addVideoWall(const QString& name) const;
    QnVideoWallItem addVideoWallScreen(
        const QString& name,
        const QnVideoWallResourcePtr& videoWall) const;
    QnVideoWallMatrix addVideoWallMatrix(
        const QString& name,
        const QnVideoWallResourcePtr& videoWall) const;
    void updateVideoWallScreen(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& screen) const;
    void updateVideoWallMatrix(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallMatrix& matrix) const;

    nx::vms::api::ShowreelData addShowreel(const QString& name,
        const nx::Uuid& parentId = nx::Uuid()) const;
    QnVirtualCameraResourcePtr addIntercomCamera(
        const QString& name,
        const nx::Uuid& parentId = nx::Uuid(),
        const QString& hostAddress = QString()) const;
    core::LayoutResourcePtr addIntercomLayout(
        const QString& name,
        const nx::Uuid& parentId = nx::Uuid()) const;

    void setupControlAllVideoWallsAccess(const QnUserResourcePtr& user) const;

    QnUserResourcePtr loginAs(const QString& name,
        const std::vector<nx::Uuid>& groupIds = {}) const;
    QnUserResourcePtr loginAsAdministrator(const QString& name) const;
    QnUserResourcePtr loginAsPowerUser(const QString& name) const;
    QnUserResourcePtr loginAsLiveViewer(const QString& name) const;
    QnUserResourcePtr loginAsAdvancedViewer(const QString& name) const;
    QnUserResourcePtr loginAsCustomUser(const QString& name) const;
    QnUserResourcePtr currentUser() const;
    void logout() const;

    void setFilterMode(entity_resource_tree::ResourceTreeComposer::FilterMode filterMode);

    QAbstractItemModel* model() const override;

protected:
    QSharedPointer<core::entity_item_model::EntityItemModel> m_newResourceTreeModel;
    QSharedPointer<entity_resource_tree::ResourceTreeComposer> m_resourceTreeComposer;
};

} // namespace test
} // namespace nx::vms::client::desktop
