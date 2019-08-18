#include "resource_tree_model_test_fixture.h"

#include <QtCore/QFileInfo>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::index_condition;

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

QnAviResourcePtr ResourceTreeModelTest::addMediaResource(const QString& path) const
{
    QnAviResourcePtr mediaResource(new QnAviResource(path, commonModule()));
    resourcePool()->addResource(mediaResource);
    return mediaResource;
}

QnFileLayoutResourcePtr ResourceTreeModelTest::addFileLayoutResource(
    const QString& path,
    bool isEncrypted) const
{
    QnFileLayoutResourcePtr fileLayoutResource(new QnFileLayoutResource(commonModule()));
    fileLayoutResource->setIdUnsafe(guidFromArbitraryData(path));
    fileLayoutResource->setUrl(path);
    fileLayoutResource->setName(QFileInfo(path).fileName());
    fileLayoutResource->setIsEncrypted(isEncrypted);
    resourcePool()->addResource(fileLayoutResource);
    return fileLayoutResource;
}

QnLayoutResourcePtr ResourceTreeModelTest::addLayoutResource(
    const QString& name,
    const QnUuid& parentId) const
{
    QnLayoutResourcePtr layoutResource(new QnLayoutResource(commonModule()));
    layoutResource->setName(name);
    layoutResource->setIdUnsafe(QnUuid::createUuid());
    layoutResource->setParentId(parentId);
    resourcePool()->addResource(layoutResource);
    return layoutResource;
}

QnWebPageResourcePtr ResourceTreeModelTest::addWebPageResource(const QString& name) const
{
    QnWebPageResourcePtr webPageResource(new QnWebPageResource());
    webPageResource->setName(name);
    webPageResource->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(webPageResource);
    return webPageResource;
}

QnVideoWallResourcePtr ResourceTreeModelTest::addVideoWallResource(const QString& name) const
{
    QnVideoWallResourcePtr videoWallResource(new QnVideoWallResource(commonModule()));
    videoWallResource->setName(name);
    videoWallResource->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(videoWallResource);
    return videoWallResource;
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

void ResourceTreeModelTest::logout() const
{
    m_accessController->setUser(QnUserResourcePtr());
    resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
}

QnUserResourcePtr ResourceTreeModelTest::loginAsAdmin(const QString& name) const
{
    logout();
    auto user = addUser(name, GlobalPermission::adminPermissions);
    m_accessController->setUser(user);
    return user;
}

QnUserResourcePtr ResourceTreeModelTest::loginAsLiveViewer(const QString& name) const
{
    logout();
    auto user = addUser(name, GlobalPermission::liveViewerPermissions);
    m_accessController->setUser(user);
    return user;
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
