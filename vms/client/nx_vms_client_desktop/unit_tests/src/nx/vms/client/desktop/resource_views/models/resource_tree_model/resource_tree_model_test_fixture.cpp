#include "resource_tree_model_test_fixture.h"

#include <QtCore/QFileInfo>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/camera_resource.h>
#include <client/client_settings.h>
#include <test_support/resource/camera_resource_stub.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::test;
using namespace nx::vms::client::desktop::test::index_condition;

void ResourceTreeModelTest::SetUp()
{
    QnStartupParameters startupParameters;
    startupParameters.skipMediaFolderScan = true;

    m_clientModule.reset(new QnClientModule(startupParameters, nullptr));
    m_accessController.reset(new QnWorkbenchAccessController(commonModule()));
    m_layoutSnapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(commonModule()));
    m_resourceTreeModel.reset(new QnResourceTreeModel(
        QnResourceTreeModel::FullScope,
        m_accessController.get(),
        m_layoutSnapshotManager.get(),
        commonModule()));

    m_resourceTreeProxyModel.reset(new QnResourceTreeSortProxyModel());
    m_resourceTreeProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_resourceTreeProxyModel->setSourceModel(m_resourceTreeModel.get());
    m_resourceTreeProxyModel->sort(Qn::NameColumn);
}

void ResourceTreeModelTest::TearDown()
{
    m_resourceTreeProxyModel.clear();
    m_resourceTreeModel.clear();
    m_layoutSnapshotManager.clear();
    m_accessController.clear();
    m_clientModule.clear();
}

QnCommonModule* ResourceTreeModelTest::commonModule() const
{
    return m_clientModule->clientCoreModule()->commonModule();
}

QnResourcePool* ResourceTreeModelTest::resourcePool() const
{
    return commonModule()->resourcePool();
}

QnWorkbenchAccessController* ResourceTreeModelTest::workbenchAccessController() const
{
    return m_accessController.get();
}

QnResourceAccessManager* ResourceTreeModelTest::resourceAccessManager() const
{
    return commonModule()->resourceAccessManager();
}

QnWorkbenchLayoutSnapshotManager* ResourceTreeModelTest::layoutSnapshotManager() const
 {
    return m_layoutSnapshotManager.get();
}

QnRuntimeInfoManager* ResourceTreeModelTest::runtimeInfoManager() const
{
    return commonModule()->runtimeInfoManager();
}

void ResourceTreeModelTest::setSystemName(const QString& name) const
{
    commonModule()->globalSettings()->setSystemName(name);
}

QnUserResourcePtr ResourceTreeModelTest::addUser(
    const QString& name,
    GlobalPermissions globalPermissions,
    QnUserType userType) const
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
    QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
    server->setIdUnsafe(QnUuid::createUuid());
    server->setStatus(Qn::ResourceStatus::Offline);
    server->setName(name);
    resourcePool()->addResource(server);
    return server;
}

QnAviResourcePtr ResourceTreeModelTest::addLocalMedia(const QString& path) const
{
    QnAviResourcePtr localMedia(new QnAviResource(path, commonModule()));
    resourcePool()->addResource(localMedia);
    return localMedia;
}

QnFileLayoutResourcePtr ResourceTreeModelTest::addFileLayout(
    const QString& path,
    bool isEncrypted) const
{
    QnFileLayoutResourcePtr fileLayout(new QnFileLayoutResource(commonModule()));
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
    QnLayoutResourcePtr layout(new QnLayoutResource(commonModule()));
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

QnVideoWallResourcePtr ResourceTreeModelTest::addVideoWall(const QString& name) const
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource(commonModule()));
    videoWall->setName(name);
    videoWall->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(videoWall);
    return videoWall;
}

void ResourceTreeModelTest::addVideoWallItem(
    const QString& name,
    QnVideoWallResourcePtr videowall) const
{
    auto layout = addLayout("layout", videowall->getId());
    layout->setData(Qn::VideoWallResourceRole, qVariantFromValue(videowall));
    QnVideoWallItem item;
    item.name = name;
    item.uuid = QnUuid::createUuid();
    item.layout = layout->getId();
    videowall->items()->addOrUpdateItem(item);
}

void ResourceTreeModelTest::addVideoWallMatrix(
    const QString& name,
    QnVideoWallResourcePtr videowall) const
{
    QnVideoWallMatrix matrix;
    matrix.name = name;
    matrix.uuid = QnUuid::createUuid();
    for (const QnVideoWallItem& item: videowall->items()->getItems())
    {
        if (item.layout.isNull() || !resourcePool()->getResourceById(item.layout))
            continue;
        matrix.layoutByItem[item.uuid] = item.layout;
    }
    videowall->matrices()->addItem(matrix);
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
    const QnUuid& parentId /*= QnUuid()*/) const
{
    QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    camera->setName(name);
    camera->setIdUnsafe(QnUuid::createUuid());
    camera->setParentId(parentId);
    resourcePool()->addResource(camera);
    return camera;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsUserWithPermissions(
    const QString& name,
    GlobalPermissions permissions) const
{
    logout();
    const auto users = resourcePool()->getResources<QnUserResource>();
    const auto itr = std::find_if(users.cbegin(), users.cend(),
        [this, name](const QnUserResourcePtr& user)
        {
            const auto permissions = resourceAccessManager()->globalPermissions(user);
            return (user->getName() == name) && permissions.testFlag(GlobalPermission::admin);
        });

    QnUserResourcePtr user;
    if (itr != users.cend())
        user = *itr;
    else
        user = addUser(name, GlobalPermission::adminPermissions);

    m_accessController->setUser(user);
    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdmin(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::admin);
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name) const
{
    return loginAsUserWithPermissions(name, GlobalPermission::liveViewerPermissions);
}

void ResourceTreeModelTest::logout() const
{
    m_accessController->setUser(QnUserResourcePtr());
}

QAbstractItemModel* ResourceTreeModelTest::model() const
{
    // TODO: #vbreus This method should return subtree model that mocking actual subtree
    // representation done by setting root node to the view (which is absent within
    // test environment).
    return m_resourceTreeProxyModel.get();
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
