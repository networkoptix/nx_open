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
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/client_camera_resource_stub.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
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
        systemContext(),
        /*resourceTreeSettings*/ nullptr));

    m_resourceTreeComposer->attachModel(m_newResourceTreeModel.get());
}

void ResourceTreeModelTest::TearDown()
{
    m_resourceTreeComposer.clear();
    m_newResourceTreeModel.clear();
    logout();
    systemContext()->accessRightsManager()->resetAccessRights({});
    resourcePool()->clear();
}

QnResourcePool* ResourceTreeModelTest::resourcePool() const
{
    return systemContext()->resourcePool();
}

ResourceTreeModelTest::AccessController* ResourceTreeModelTest::accessController() const
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
    const QString& name, const std::optional<QnUuid>& groupId) const
{
    QnUserResourcePtr user(new QnUserResource(nx::vms::api::UserType::local, /*externalId*/ {}));
    user->setIdUnsafe(QnUuid::createUuid());
    user->setName(name);
    if (groupId)
        user->setGroupIds({*groupId});
    user->addFlags(Qn::remote);
    resourcePool()->addResource(user);
    return user;
}

ServerResourcePtr ResourceTreeModelTest::addServer(const QString& name) const
{
    ServerResourcePtr server(new ServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setName(name);
    resourcePool()->addResource(server);
    return server;
}

QnMediaServerResourcePtr ResourceTreeModelTest::addEdgeServer(const QString& name, const QString& address) const
{
    QnMediaServerResourcePtr server(new ServerResource());
    server->setIdUnsafe(QnUuid::createUuid());
    server->setName(name);
    server->setServerFlags(server->getServerFlags() | ServerFlag::SF_Edge);
    server->setNetAddrList({nx::network::SocketAddress(address.toStdString())});
    resourcePool()->addResource(server);
    return server;
}

QnMediaServerResourcePtr ResourceTreeModelTest::addFakeServer(const QString& name) const
{
    QnMediaServerResourcePtr server(new QnFakeMediaServerResource());
    server->setName(name);
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
    QnFileLayoutResourcePtr fileLayout(new QnFileLayoutResource({}));
    fileLayout->setIdUnsafe(guidFromArbitraryData(path));
    fileLayout->setUrl(path);
    fileLayout->setName(QFileInfo(path).fileName());
    fileLayout->setIsEncrypted(isEncrypted);
    resourcePool()->addResource(fileLayout);
    return fileLayout;
}

LayoutResourcePtr ResourceTreeModelTest::addLayout(
    const QString& name,
    const QnUuid& parentId) const
{
    LayoutResourcePtr layout(new LayoutResource());
    layout->setName(name);
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);
    return layout;
}

QnWebPageResourcePtr ResourceTreeModelTest::addWebPage(
    const QString& name,
    WebPageSubtype subtype) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(webPage);

    // Properties can be set only after Resource is added to the Resource Pool.
    webPage->setSubtype(subtype);

    return webPage;
}

QnWebPageResourcePtr ResourceTreeModelTest::addProxiedWebPage(
    const QString& name,
    WebPageSubtype subtype,
    const QnUuid& serverId) const
{
    QnWebPageResourcePtr webPage(new QnWebPageResource());
    webPage->setName(name);
    webPage->setIdUnsafe(QnUuid::createUuid());
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

ShowreelData ResourceTreeModelTest::addShowreel(
    const QString& name,
    const QnUuid& parentId) const
{
    ShowreelData showreel;
    showreel.id = QnUuid::createUuid();
    showreel.parentId = parentId;
    showreel.name = name;
    systemContext()->showreelManager()->addOrUpdateShowreel(showreel);
    return showreel;
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
    QnClientCameraResourcePtr camera(new ClientCameraResourceStub());
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

LayoutResourcePtr ResourceTreeModelTest::addIntercomLayout(
    const QString& name,
    const QnUuid& parentId) const
{
    LayoutResourcePtr layout(new LayoutResource());
    layout->setName(name);
    layout->setIdUnsafe(nx::vms::common::calculateIntercomLayoutId(parentId));
    layout->setParentId(parentId);
    resourcePool()->addResource(layout);

    if (parentId.isNull())
    {
        NX_ASSERT(!nx::vms::common::isIntercomLayout(layout));
        NX_ASSERT(layout->layoutType() != LayoutResource::LayoutType::intercom);
    }
    else
    {
        NX_ASSERT(nx::vms::common::isIntercomLayout(layout));
        NX_ASSERT(layout->layoutType() == LayoutResource::LayoutType::intercom);
    }

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
        common::LayoutItemData layoutItemData;
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
    setupAccessToResourceForUser(user, resource, isAccessible
        ? kViewAccessRights
        : kNoAccessRights);
}

void ResourceTreeModelTest::setupAccessToResourceForUser(
    const QnUserResourcePtr& user,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRights accessRights) const
{
    if (NX_ASSERT(resource))
        setupAccessToObjectForUser(user, resource->getId(), accessRights);
}

void ResourceTreeModelTest::setupAccessToObjectForUser(
    const QnUserResourcePtr& user,
    const QnUuid& resourceOrGroupId,
    nx::vms::api::AccessRights accessRights) const
{
    if (!NX_ASSERT(user && !resourceAccessManager()->hasPowerUserPermissions(user)))
        return;

    const auto userId = user->getId();
    const auto accessRightsManager = commonModule()->systemContext()->accessRightsManager();

    auto accessRightsMap = accessRightsManager->ownResourceAccessMap(userId);
    if (accessRights != 0)
        accessRightsMap.emplace(resourceOrGroupId, accessRights);
    else
        accessRightsMap.remove(resourceOrGroupId);

    accessRightsManager->setOwnResourceAccessMap(userId, accessRightsMap);
}

void ResourceTreeModelTest::setupAllMediaAccess(
    const QnUserResourcePtr& user, AccessRights accessRights) const
{
    setupAccessToObjectForUser(user, kAllDevicesGroupId, accessRights);
    setupAccessToObjectForUser(user, kAllWebPagesGroupId, accessRights & AccessRight::view);
    setupAccessToObjectForUser(user, kAllServersGroupId, accessRights & AccessRight::view);
}

void ResourceTreeModelTest::setupControlAllVideoWallsAccess(const QnUserResourcePtr& user) const
{
    setupAccessToObjectForUser(user, kAllVideoWallsGroupId, AccessRight::edit);
}

QnUserResourcePtr ResourceTreeModelTest::loginAs(
    const QString& name, const std::optional<QnUuid>& groupId) const
{
    logout();
    const auto users = resourcePool()->getResources<QnUserResource>();

    const std::vector<QnUuid> groupIds = groupId
        ? std::vector<QnUuid>{*groupId}
        : std::vector<QnUuid>{};

    const auto itr = std::find_if(users.cbegin(), users.cend(),
        [this, name, &groupIds](const QnUserResourcePtr& user)
        {
            return user->getName() == name && user->groupIds() == groupIds;
        });

    QnUserResourcePtr user;
    if (itr != users.cend())
        user = *itr;
    else
        user = addUser(name, groupId);

    accessController()->setUser(user);
    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdministrator(const QString& name) const
{
    return loginAs(name, api::kAdministratorsGroupId);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsPowerUser(const QString& name) const
{
    return loginAs(name, api::kPowerUsersGroupId);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name) const
{
    return loginAs(name, api::kLiveViewersGroupId);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdvancedViewer(const QString& name) const
{
    return loginAs(name, api::kAdvancedViewersGroupId);
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

INSTANTIATE_TEST_SUITE_P(
    WebPageSubtype,
    ResourceTreeModelTest,
    ::testing::Values(WebPageSubtype::none, WebPageSubtype::clientApi));

} // namespace test
} // namespace nx::vms::client::desktop
