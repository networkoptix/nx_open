// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QHash>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_model_adapter.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

class AccessSubjectEditingFixture: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();
        createTestResources();
        loginAs(nx::vms::api::kPowerUsersGroupId);
        createResourceTreeModel();
        editingContext.reset(new AccessSubjectEditingContext(systemContext()));
        editingContext->setCurrentSubject(QnUuid::createUuid(),
            AccessSubjectEditingContext::SubjectType::user);
    }

    virtual void TearDown() override
    {
        logout();
        testResources.clear();
        editingContext.reset();
        resourceTreeModel.reset();
        ContextBasedTest::TearDown();
    }

    QnResourcePtr resource(const QString& uniqueName) const
    {
        return testResources.value(uniqueName);
    }

    QModelIndex indexOf(const QnResourcePtr& resource) const
    {
        return findIndexRecursively({}, core::ResourceRole, QVariant::fromValue(resource));
    }

    QModelIndex indexOf(ResourceTree::NodeType nodeType) const
    {
        return findIndexRecursively({}, Qn::NodeTypeRole, QVariant::fromValue(nodeType));
    }

    QModelIndex indexOf(const QString& displayedName) const
    {
        return findIndexRecursively({}, Qt::DisplayRole, displayedName);
    }

private:
    void createTestResources()
    {
        /*
        Cameras & Devices
            Group1
                Camera1
                Camera2
            Camera3
            Camera4
            Camera5

        Layouts
            Layout1
            Layout2

        Web Pages & Integrations
            WebPage1
            WebPage2

        Health Monitors
            Server1

        Video Walls
            VideoWall1
            VideoWall2
        */

        addTestResource(addCamera(), "Camera1");
        addTestResource(addCamera(), "Camera2");
        addTestResource(addCamera(), "Camera3");
        addTestResource(addCamera(), "Camera4");
        addTestResource(addCamera(), "Camera5");
        addTestResource(addLayout(), "Layout1");
        addTestResource(addLayout(), "Layout2");
        addTestResource(addWebPage(), "WebPage1");
        addTestResource(addWebPage(), "WebPage2");
        addTestResource(addServer(), "Server1");
        addTestResource(addVideoWall(), "VideoWall1");
        addTestResource(addVideoWall(), "VideoWall2");

        entity_resource_tree::resource_grouping::setResourceCustomGroupId(
            testResources.value("Camera1"), "Group1");

        entity_resource_tree::resource_grouping::setResourceCustomGroupId(
            testResources.value("Camera2"), "Group1");
    }

    void addTestResource(const QnResourcePtr& resource, const QString& uniqueName)
    {
        ASSERT_FALSE(testResources.contains(uniqueName));
        resource->setName(uniqueName);
        testResources.insert(uniqueName, resource);
    }

    void createResourceTreeModel()
    {
        resourceTreeModel.reset(new ResourceSelectionModelAdapter());

        resourceTreeModel->setResourceTypes(
            ResourceTree::ResourceFilter::camerasAndDevices
            | ResourceTree::ResourceFilter::webPages
            | ResourceTree::ResourceFilter::integrations
            | ResourceTree::ResourceFilter::layouts
            | ResourceTree::ResourceFilter::healthMonitors
            | ResourceTree::ResourceFilter::videoWalls);

        resourceTreeModel->setWebPagesAndIntegrationsCombined(true);

        resourceTreeModel->setTopLevelNodesPolicy(
            ResourceSelectionModelAdapter::TopLevelNodesPolicy::showAlways);

        resourceTreeModel->setContext(systemContext());
    }

    QModelIndex findIndexRecursively(
        const QModelIndex& parent, int role, const QVariant& value) const
    {
        if (!resourceTreeModel->hasChildren(parent))
            return {};

        const auto result = resourceTreeModel->match(resourceTreeModel->index(0, 0, parent),
            role, value, 1, Qt::MatchExactly | Qt::MatchRecursive);

        return result.empty() ? QModelIndex{} : result[0];
    }

    void logout()
    {
        systemContext()->accessController()->setUser({});
    }

    void loginAs(Ids groupIds, const QString& name = "user")
    {
        auto user = createUser(groupIds, name);
        resourcePool()->addResource(user);
        systemContext()->accessController()->setUser(user);
    }

protected:
    std::unique_ptr<ResourceSelectionModelAdapter> resourceTreeModel;
    std::unique_ptr<AccessSubjectEditingContext> editingContext;
    QHash<QString, QnResourcePtr> testResources;
};

} // namespace test
} // namespace nx::vms::client::desktop
