#include "resource_tree_model_test_fixture.h"

#include <QtCore/QFileInfo>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>

using namespace nx;
using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

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

void ResourceTreeModelTest::logout() const
{
    m_accessController->setUser(QnUserResourcePtr());
    resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
}

void ResourceTreeModelTest::loginAsAdmin(const QString& name) const
{
    logout();
    auto user = addUser(name, GlobalPermission::adminPermissions);
    m_accessController->setUser(user);
}

QAbstractItemModel* ResourceTreeModelTest::model() const
{
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

QModelIndexList ResourceTreeModelTest::getIndexByData(const KeyValueVector& lookupData) const
{
    const auto dataMatch =
        [lookupData](const QModelIndex& index) -> bool
        {
            for (const auto& keyValue: lookupData)
            {
                const auto indexData = index.data(keyValue.first);
                if (indexData.isNull() || indexData != keyValue.second)
                    return false;
            }
            return true;
        };

    QModelIndexList result;
    const auto allIndexes = getAllIndexes();
    std::copy_if(allIndexes.cbegin(), allIndexes.cend(), std::back_inserter(result), dataMatch);

    return result;
}

QJsonDocument ResourceTreeModelTest::testSnapshot(
    ItemModelStateSnapshotHelper::SnapshotParams& params) const
{
    return ItemModelStateSnapshotHelper::makeSnapshot(model(), params);
}

std::string ResourceTreeModelTest::snapshotsOutputString(const QJsonDocument& actual,
    const QJsonDocument& reference) const
{
    return QString("Actual snapshot:\n%1Reference snapshot:\n%2")
        .arg(QString(actual.toJson()))
        .arg(QString(reference.toJson()))
        .toStdString();
}
