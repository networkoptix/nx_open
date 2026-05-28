// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <QtCore/QFileInfo>
#include <QtTest/QAbstractItemModelTester>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/client_camera_resource_stub.h>
#include <nx/vms/client/desktop/test_support/message_processor_mock.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/camera_resource_stub.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <utils/common/id.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api;
using namespace core::test::index_condition;
using namespace nx::vms::client::core;

void ResourceTreeModelTest::SetUp()
{
    // Should be not null for correct Videowall Item node display.
    ASSERT_FALSE(peerId().isNull());

    m_newResourceTreeModel.reset(new entity_item_model::EntityItemModel());
    nx::utils::ModelTransactionChecker::install(m_newResourceTreeModel.get());

    // Create QAbstractItemModelTester owned by the model.
    new QAbstractItemModelTester(m_newResourceTreeModel.get(),
        QAbstractItemModelTester::FailureReportingMode::Fatal,
        /*parent*/ m_newResourceTreeModel.get());

    m_resourceTreeComposer.reset(new entity_resource_tree::ResourceTreeComposer(
        systemContext(),
        /*resourceTreeSettings*/ nullptr));

    m_resourceTreeComposer->attachModel(m_newResourceTreeModel.get());

    createMessageProcessor();
}

void ResourceTreeModelTest::TearDown()
{
    m_resourceTreeComposer.clear();
    m_newResourceTreeModel.clear();
    logout();
    SystemContextBasedTest::TearDown();
}

ResourceTreeModelTest::AccessController* ResourceTreeModelTest::accessController() const
{
    return systemContext()->accessController();
}

void ResourceTreeModelTest::addOtherServer(const QString& name) const
{
    auto messageProcessor = dynamic_cast<MessageProcessorMock*>(systemContext()->messageProcessor());

    nx::vms::api::DiscoveredServerData discoveredServer;
    discoveredServer.name = name;
    discoveredServer.id = nx::Uuid::createUuid();
    discoveredServer.status = nx::vms::api::ResourceStatus::incompatible;
    discoveredServer.version = nx::utils::SoftwareVersion(2, 3, 0, 1); //< Min suitable version.

    messageProcessor->emulateConnectionOpened();
    messageProcessor->emulatateServerInitiallyDiscovered(discoveredServer);
}

QnAviResourcePtr ResourceTreeModelTest::addLocalMedia(const QString& path) const
{
    QnAviResourcePtr localMedia(new QnAviResource(path));
    if (FileTypeSupport::isImageFileExt(path))
    {
        localMedia->addFlags(Qn::still_image);
        localMedia->removeFlags(Qn::video | Qn::audio);
    }
    localMedia->setStatus(nx::vms::api::ResourceStatus::online);
    resourcePool()->addResource(localMedia);
    return localMedia;
}

QnFileLayoutResourcePtr ResourceTreeModelTest::addFileLayout(
    const QString& path,
    bool isEncrypted) const
{
    QnFileLayoutResourcePtr fileLayout(new QnFileLayoutResource({}));
    fileLayout->setIdUnsafe(guidFromArbitraryData(path));
    fileLayout->setUrl(path);
    fileLayout->setName(QFileInfo(path).fileName());
    fileLayout->setIsEncrypted(isEncrypted);
    resourcePool()->addResource(fileLayout);
    return fileLayout;
}

QnWebPageResourcePtr ResourceTreeModelTest::addWebPage(
    const QString& name,
    WebPageSubtype subtype) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(nx::Uuid::createUuid());
    resourcePool()->addResource(webPage);

    // Properties can be set only after Resource is added to the Resource Pool.
    webPage->setSubtype(subtype);

    return webPage;
}

QnWebPageResourcePtr ResourceTreeModelTest::addProxiedWebPage(
    const QString& name,
    WebPageSubtype subtype,
    const nx::Uuid& serverId) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(nx::Uuid::createUuid());
    webPage->setProxyId(serverId);
    resourcePool()->addResource(webPage);

    // Properties can be set only after Resource is added to the Resource Pool.
    webPage->setSubtype(subtype);

    return webPage;
}

QnVideoWallResourcePtr ResourceTreeModelTest::addVideoWall(const QString& name) const
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setName(name);
    videoWall->setIdUnsafe(nx::Uuid::createUuid());
    resourcePool()->addResource(videoWall);
    return videoWall;
}

QnVideoWallItem ResourceTreeModelTest::addVideoWallScreen(
    const QString& name,
    const QnVideoWallResourcePtr& videoWall) const
{
    auto layout = addLayout("layout", videoWall->getId()).dynamicCast<LayoutResource>();
    layout->setData(Qn::VideoWallResourceRole, QVariant::fromValue(videoWall));
    QnVideoWallItem screen;
    screen.name = name;
    screen.uuid = nx::Uuid::createUuid();
    screen.layout = layout->getId();
    videoWall->items()->addOrUpdateItem(screen);
    return screen;
}

QnVideoWallMatrix ResourceTreeModelTest::addVideoWallMatrix(
    const QString& name,
    const QnVideoWallResourcePtr& videoWall) const
{
    QnVideoWallMatrix matrix;
    matrix.name = name;
    matrix.uuid = nx::Uuid::createUuid();
    for (const QnVideoWallItem& item: videoWall->items()->getItems())
    {
        if (item.layout.isNull() || !resourcePool()->getResourceById(item.layout))
            continue;
        matrix.layoutByItem[item.uuid] = item.layout;
    }
    videoWall->matrices()->addItem(matrix);
    return matrix;
}

void ResourceTreeModelTest::updateVideoWallScreen(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& screen) const
{
    if (NX_ASSERT(videoWall->items()->hasItem(screen.uuid)))
        videoWall->items()->addOrUpdateItem(screen);
}

void ResourceTreeModelTest::updateVideoWallMatrix(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallMatrix& matrix) const
{
    if (NX_ASSERT(videoWall->matrices()->hasItem(matrix.uuid)))
        videoWall->matrices()->addOrUpdateItem(matrix);
}

ShowreelData ResourceTreeModelTest::addShowreel(
    const QString& name,
    const nx::Uuid& parentId) const
{
    ShowreelData showreel;
    showreel.id = nx::Uuid::createUuid();
    showreel.parentId = parentId;
    showreel.name = name;
    systemContext()->showreelManager()->addOrUpdateShowreel(showreel);
    return showreel;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addIntercomCamera(
    const QString& name,
    const nx::Uuid& parentId,
    const QString& hostAddress) const
{
    nx::vms::client::core::CameraResourcePtr camera(new ClientCameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(nx::Uuid::createUuid());
    camera->setParentId(parentId);
    if (!hostAddress.isEmpty())
        camera->setHostAddress(hostAddress);

    QnIOPortData intercomFeaturePort;
    intercomFeaturePort.outputName = QnVirtualCameraResource::intercomSpecificPortName();

    camera->setIoPortDescriptions({intercomFeaturePort}, false);

    resourcePool()->addResource(camera);
    return camera;
}

core::LayoutResourcePtr ResourceTreeModelTest::addIntercomLayout(
    const QString& name,
    const nx::Uuid& parentId) const
{
    core::LayoutResourcePtr layout(new core::LayoutResource());
    layout->setName(name);
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(parentId));
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);

    if (parentId.isNull())
    {
        NX_ASSERT(!nx::vms::common::isIntercomLayout(layout));
        NX_ASSERT(layout->layoutType() != core::LayoutResource::LayoutType::intercom);
    }
    else
    {
        NX_ASSERT(nx::vms::common::isIntercomLayout(layout));
        NX_ASSERT(layout->layoutType() == core::LayoutResource::LayoutType::intercom);
    }

    return layout;
}

void ResourceTreeModelTest::setupControlAllVideoWallsAccess(const QnUserResourcePtr& user) const
{
    setupAccessToObjectForUser(user, kAllVideoWallsGroupId, AccessRight::edit);
}

QnUserResourcePtr ResourceTreeModelTest::loginAs(
    const QString& name, const std::vector<nx::Uuid>& groupIds) const
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
    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdministrator(const QString& name) const
{
    return loginAs(name, {api::kAdministratorsGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsPowerUser(const QString& name) const
{
    return loginAs(name, {api::kPowerUsersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name) const
{
    return loginAs(name, {api::kLiveViewersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdvancedViewer(const QString& name) const
{
    return loginAs(name, {api::kAdvancedViewersGroupId});
}

QnUserResourcePtr ResourceTreeModelTest::loginAsCustomUser(const QString& name) const
{
    return loginAs(name);
}

QnUserResourcePtr ResourceTreeModelTest::currentUser() const
{
    return accessController()->user();
}

void ResourceTreeModelTest::logout() const
{
    accessController()->setUser(QnUserResourcePtr());
}

void ResourceTreeModelTest::setFilterMode(
    entity_resource_tree::ResourceTreeComposer::FilterMode filterMode)
{
    m_resourceTreeComposer->setFilterMode(filterMode);
}

QAbstractItemModel* ResourceTreeModelTest::model() const
{
    return m_newResourceTreeModel.get();
}

INSTANTIATE_TEST_SUITE_P(
    WebPageSubtype,
    ResourceTreeModelTest,
    ::testing::Values(WebPageSubtype::none, WebPageSubtype::clientApi));

} // namespace test
} // namespace nx::vms::client::desktop
