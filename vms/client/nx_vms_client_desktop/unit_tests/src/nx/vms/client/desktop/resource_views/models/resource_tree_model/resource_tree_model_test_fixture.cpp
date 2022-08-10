// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_model_test_fixture.h"

#include <QtCore/QFileInfo>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/filetypesupport.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resources/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <ui/workbench/workbench_access_controller.h>
#include <utils/common/id.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api;
using namespace index_condition;

void ResourceTreeModelTest::SetUp()
{
    // Should be not null for correct Videowall Item node display.
    ASSERT_FALSE(systemContext()->peerId().isNull());

    m_newResourceTreeModel.reset(new entity_item_model::EntityItemModel());
    nx::utils::ModelTransactionChecker::install(m_newResourceTreeModel.get());

    m_resourceTreeComposer.reset(new entity_resource_tree::ResourceTreeComposer(
        commonModule(),
        systemContext()->accessController(),
        nullptr));
    m_resourceTreeComposer->attachModel(m_newResourceTreeModel.get());
}

void ResourceTreeModelTest::TearDown()
{
    m_resourceTreeComposer.clear();
    m_newResourceTreeModel.clear();
}

QnResourcePool* ResourceTreeModelTest::resourcePool() const
{
    return systemContext()->resourcePool();
}

QnWorkbenchAccessController* ResourceTreeModelTest::workbenchAccessController() const
{
    return systemContext()->accessController();
}

QnResourceAccessManager* ResourceTreeModelTest::resourceAccessManager() const
{
    return systemContext()->resourceAccessManager();
}

LayoutSnapshotManager* ResourceTreeModelTest::layoutSnapshotManager() const
 {
    return systemContext()->layoutSnapshotManager();
}

QnRuntimeInfoManager* ResourceTreeModelTest::runtimeInfoManager() const
{
    return systemContext()->runtimeInfoManager();
}

void ResourceTreeModelTest::setSystemName(const QString& name) const
{
    systemContext()->globalSettings()->setSystemName(name);
}

QnUserResourcePtr ResourceTreeModelTest::addUser(
    const QString& name,
    GlobalPermissions globalPermissions,
    nx::vms::api::UserType userType) const
{
    QnUserResourcePtr user(new QnUserResource(userType));
    user->setIdUnsafe(QnUuid::createUuid());
    user->setName(name);
    user->setRawPermissions(globalPermissions);
    user->addFlags(Qn::remote);
    resourcePool()->addResource(user);
    return user;
}

QnMediaServerResourcePtr ResourceTreeModelTest::addServer(const QString& name) const
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setName(name);
    resourcePool()->addResource(server);
    return server;
}

QnMediaServerResourcePtr ResourceTreeModelTest::addEdgeServer(const QString& name, const QString& address) const
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setName(name);
    server->setServerFlags(server->getServerFlags() | ServerFlag::SF_Edge);
    server->setNetAddrList({nx::network::SocketAddress(address.toStdString())});
    resourcePool()->addResource(server);
    return server;
}

QnMediaServerResourcePtr ResourceTreeModelTest::addFakeServer(const QString& name) const
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setName(name);
    server->setFlags(Qn::ResourceFlag::fake_server);
    resourcePool()->addResource(server);
    return server;
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
    QnFileLayoutResourcePtr fileLayout(new QnFileLayoutResource());
    fileLayout->setIdUnsafe(guidFromArbitraryData(path));
    fileLayout->setUrl(path);
    fileLayout->setName(QFileInfo(path).fileName());
    fileLayout->setIsEncrypted(isEncrypted);
    resourcePool()->addResource(fileLayout);
    return fileLayout;
}

QnLayoutResourcePtr ResourceTreeModelTest::addLayout(
    const QString& name,
    const QnUuid& parentId) const
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setName(name);
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);
    return layout;
}

QnWebPageResourcePtr ResourceTreeModelTest::addWebPage(const QString& name) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(webPage);
    return webPage;
}

QnWebPageResourcePtr ResourceTreeModelTest::addProxiedWebResource(
    const QString& name,
    const QnUuid& serverId) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(QnUuid::createUuid());
    webPage->setProxyId(serverId);
    resourcePool()->addResource(webPage);
    return webPage;
}

QnVideoWallResourcePtr ResourceTreeModelTest::addVideoWall(const QString& name) const
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setName(name);
    videoWall->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(videoWall);
    return videoWall;
}

QnVideoWallItem ResourceTreeModelTest::addVideoWallScreen(
    const QString& name,
    const QnVideoWallResourcePtr& videoWall) const
{
    auto layout = addLayout("layout", videoWall->getId());
    layout->setData(Qn::VideoWallResourceRole, QVariant::fromValue(videoWall));
    QnVideoWallItem screen;
    screen.name = name;
    screen.uuid = QnUuid::createUuid();
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
    matrix.uuid = QnUuid::createUuid();
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

LayoutTourData ResourceTreeModelTest::addLayoutTour(
    const QString& name,
    const QnUuid& parentId) const
{
    LayoutTourData tourData;
    tourData.id = QnUuid::createUuid();
    tourData.parentId = parentId;
    tourData.name = name;
    commonModule()->layoutTourManager()->addOrUpdateTour(tourData);
    return tourData;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addCamera(
    const QString& name,
    const QnUuid& parentId,
    const QString& hostAddress) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    if (!hostAddress.isEmpty())
        camera->setHostAddress(hostAddress);
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addEdgeCamera(
    const QString& name, const QnMediaServerResourcePtr& edgeServer) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(edgeServer->getId());
    if (NX_ASSERT(!edgeServer->getNetAddrList().empty(), "Edge server should have some network address"))
        camera->setHostAddress(toQString(edgeServer->getNetAddrList().first().address.toString()));
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addVirtualCamera(
    const QString& name,
    const QnUuid& parentId /*= QnUuid()*/) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    camera->setFlags(camera->flags() | Qn::ResourceFlag::virtual_camera);
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addIOModule(
    const QString& name,
    const QnUuid& parentId /*= QnUuid()*/) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    camera->setFlags(camera->flags() | Qn::ResourceFlag::io_module);
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addRecorderCamera(
    const QString& name,
    const QString& groupId,
    const QnUuid& parentId /*= QnUuid()*/) const
{
    CameraResourceStubPtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    camera->setGroupId(groupId);
    camera->markCameraAsNvr();
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addMultisensorSubCamera(
    const QString& name,
    const QString& groupId,
    const QnUuid& parentId /*= QnUuid()*/) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    camera->setGroupId(groupId);
    resourcePool()->addResource(camera);
    return camera;
}

QnVirtualCameraResourcePtr ResourceTreeModelTest::addIntercomCamera(
    const QString& name,
    const QnUuid& parentId,
    const QString& hostAddress) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    if (!hostAddress.isEmpty())
        camera->setHostAddress(hostAddress);

    static const QString kOpenDoorPortName =
        QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));

    QnIOPortData intercomFeaturePort;
    intercomFeaturePort.outputName = kOpenDoorPortName;

    camera->setIoPortDescriptions({intercomFeaturePort}, false);

    resourcePool()->addResource(camera);
    return camera;
}

QnLayoutResourcePtr ResourceTreeModelTest::addIntercomLayout(
    const QString& name,
    const QnUuid& parentId) const
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setName(name);
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(parentId));
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);
    return layout;
}

void ResourceTreeModelTest::removeCamera(const QnVirtualCameraResourcePtr& camera) const
{
    resourcePool()->removeResource(camera);
}

void ResourceTreeModelTest::addToLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& resources) const
{
    for (const auto& resource: resources)
    {
        QnLayoutItemData layoutItemData;
        layoutItemData.resource.id = resource->getId();
        layoutItemData.uuid = QnUuid::createUuid();
        layout->addItem(layoutItemData);
    }
}

void ResourceTreeModelTest::setVideoWallScreenRuntimeStatus(
    const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& videoWallScreen,
    bool isOnline,
    const QnUuid& controlledBy)
{
    QnVideoWallItem updatedVideoWallScreen = videoWallScreen;
    updatedVideoWallScreen.runtimeStatus.online = isOnline;
    updatedVideoWallScreen.runtimeStatus.controlledBy = controlledBy;
    videoWall->items()->updateItem(updatedVideoWallScreen);
}

void ResourceTreeModelTest::setupAccessToResourceForUser(
    const QnUserResourcePtr& user,
    const QnResourcePtr& resource,
    bool isAccessible) const
{
    NX_ASSERT(!user->getRawPermissions().testFlag(GlobalPermission::admin));

    const auto sharedResourcesManager = commonModule()->sharedResourcesManager();
    auto sharedResources = sharedResourcesManager->sharedResources(user);

    if (isAccessible)
        sharedResources.insert(resource->getId());
    else
        sharedResources.remove(resource->getId());

    sharedResourcesManager->setSharedResources(user, sharedResources);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsUserWithPermissions(
    const QString& name,
    GlobalPermissions permissions,
    bool isOwner) const
{
    logout();
    const auto users = resourcePool()->getResources<QnUserResource>();
    const auto itr = std::find_if(users.cbegin(), users.cend(),
        [this, name, permissions](const QnUserResourcePtr& user)
        {
            const auto permissionsForUser = resourceAccessManager()->globalPermissions(user);
            return (user->getName() == name) && permissionsForUser == permissions;
        });

    QnUserResourcePtr user;
    if (itr != users.cend())
        user = *itr;
    else
        user = addUser(name, permissions);

    if (isOwner)
    {
        NX_ASSERT(permissions.testFlag(GlobalPermission::adminPermissions));
        user->setOwner(true);
    }

    systemContext()->accessController()->setUser(user);
    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsOwner(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::adminPermissions, /*isOwner*/ true);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdmin(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::adminPermissions);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::liveViewerPermissions);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdvancedViewer(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::advancedViewerPermissions);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsCustomUser(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::customUser);
}

void ResourceTreeModelTest::logout() const
{
    systemContext()->accessController()->setUser(QnUserResourcePtr());
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

QModelIndexList ResourceTreeModelTest::getAllIndexes() const
{
    std::function<QModelIndexList(const QModelIndex&)> getChildren;
    getChildren =
        [this, &getChildren](const QModelIndex& parent)
        {
            QModelIndexList result;
            for (int row = 0; row < model()->rowCount(parent); ++row)
            {
                const auto childIndex = model()->index(row, 0, parent);
                result.append(childIndex);
                result.append(getChildren(childIndex));
            }
            return result;
        };
    return getChildren(QModelIndex());
}

QModelIndex ResourceTreeModelTest::firstMatchingIndex(index_condition::Condition condition) const
{
    const auto matchingIndexes = allMatchingIndexes(condition);
    if (!NX_ASSERT(matchingIndexes.size() > 0))
        return QModelIndex();
    return matchingIndexes.first();
}

QModelIndex ResourceTreeModelTest::uniqueMatchingIndex(Condition condition) const
{
    const auto matchingIndexes = allMatchingIndexes(condition);
    if (!NX_ASSERT(matchingIndexes.size() == 1))
        return QModelIndex();
    return matchingIndexes.first();
}

QModelIndexList ResourceTreeModelTest::allMatchingIndexes(Condition condition) const
{
    QModelIndexList result;
    const auto allIndexes = getAllIndexes();
    std::copy_if(allIndexes.cbegin(), allIndexes.cend(), std::back_inserter(result), condition);
    return result;
}

bool ResourceTreeModelTest::noneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).isEmpty();
}

bool ResourceTreeModelTest::onlyOneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).size() == 1;
}

bool ResourceTreeModelTest::atLeastOneMatches(Condition condition) const
{
    return allMatchingIndexes(condition).size() > 0;
}

int ResourceTreeModelTest::matchCount(Condition condition) const
{
    return allMatchingIndexes(condition).size();
}

std::vector<QString> ResourceTreeModelTest::transformToDisplayStrings(
    const QModelIndexList& indexes) const
{
    std::vector<QString> result;
    std::transform(indexes.begin(), indexes.end(), std::back_inserter(result),
        [](const QModelIndex& index) { return index.data(Qt::DisplayRole).toString(); });
    return result;
}

} // namespace test
} // namespace nx::vms::client::desktop
