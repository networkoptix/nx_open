#include "media_server_process_aux.h"

#include <api/global_settings.h>
#include <common/common_module_aware.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace mserver_aux {

UnmountedLocalStoragesFilter::UnmountedLocalStoragesFilter(const QString& mediaFolderName):
    m_mediaFolderName(mediaFolderName)
{}

QString UnmountedLocalStoragesFilter::stripMediaFolderFromPath(const QString& path)
{
    if (!path.endsWith(m_mediaFolderName))
        return path;

    int indexBeforeMediaFolderName = path.indexOf(m_mediaFolderName) - 1;
    NX_ASSERT(indexBeforeMediaFolderName > 0);
    if (indexBeforeMediaFolderName <= 0)
        return path;

    return path.mid(0, indexBeforeMediaFolderName);
}

QStringList UnmountedLocalStoragesFilter::stripMediaFolderFromPaths(const QStringList& paths)
{
    QStringList result;
    std::transform(
          paths.cbegin(),
          paths.cend(),
          std::back_inserter(result),
          [this](const QString& path)
          {
            return stripMediaFolderFromPath(path);
          });

    return result;
}

QnStorageResourceList UnmountedLocalStoragesFilter::getUnmountedStorages(
        const QnStorageResourceList& allStorages,
        const QStringList& paths)
{
    QStringList strippedPaths = stripMediaFolderFromPaths(paths);
    QnStorageResourceList result;

    for (const auto& storage: allStorages)
    {
        if (!strippedPaths.contains(stripMediaFolderFromPath(storage->getUrl())))
            result.append(storage);
    }

    return result;
}


LocalSystemIndentityHelper::LocalSystemIndentityHelper(
        const BeforeRestoreDbData& restoreData,
        SystemNameProxyPtr systemName,
        const SettingsProxy* settings):
    m_restoreData(restoreData),
    m_systemName(std::move(systemName)),
    m_settings(settings)
{
    initSystemName();
    initLocalSystemId();
    m_systemName->clearFromConfig();
}

void LocalSystemIndentityHelper::initSystemName()
{
    if (!m_restoreData.localSystemName.isNull())
    {
        m_systemNameString = QString::fromLocal8Bit(m_restoreData.localSystemName);
        return;
    }

    m_systemName->loadFromConfig();
    if (m_systemName->value().isEmpty())
        m_systemName->resetToDefault();

    m_systemNameString = m_systemName->value();
}

void LocalSystemIndentityHelper::initLocalSystemId()
{
    if (!m_restoreData.localSystemId.isNull())
    {
        m_localSystemId = QnUuid::fromStringSafe(m_restoreData.localSystemId);
        return;
    }

    m_localSystemId = generateLocalSystemId();
}

QString LocalSystemIndentityHelper::getSystemNameString() const
{
    return m_systemNameString;
}

QnUuid LocalSystemIndentityHelper::getLocalSystemId() const
{
    return m_localSystemId;
}

QnUuid LocalSystemIndentityHelper::generateLocalSystemId() const
{
    if (m_settings->isSystemIdFromSystemName())
        return guidFromArbitraryData(m_systemNameString);

    if (m_systemName->isDefault())
        return QnUuid();

    return guidFromArbitraryData(m_systemNameString + m_settings->getMaxServerKey());
}

class ServerSystemNameProxy : public SystemNameProxy
{
public:
    virtual void loadFromConfig() override
    {
        m_systemName.loadFromConfig();
    }

    virtual void clearFromConfig() override
    {
        nx::SystemName().saveToConfig();
    }

    virtual void resetToDefault() override
    {
        m_systemName.resetToDefault();
    }


    virtual bool isDefault() const override
    {
        return m_systemName.isDefault();
    }

    virtual QString value() const override
    {
        return m_systemName.value();
    }

private:
    nx::SystemName m_systemName;
};

SystemNameProxyPtr createServerSystemNameProxy()
{
    return std::unique_ptr<SystemNameProxy>(new ServerSystemNameProxy);
}

class ServerSettingsProxy: public SettingsProxy, public QnCommonModuleAware
{
public:
    ServerSettingsProxy(QnCommonModule* commonModule):
        SettingsProxy(),
        QnCommonModuleAware(commonModule)
    {

    }

    virtual QString systemName() const override
    {
        return qnGlobalSettings->systemName();
    }

    virtual QnUuid localSystemId() const override
    {
        return qnGlobalSettings->localSystemId();
    }

    virtual void setSystemName(const QString& systemName) override
    {
        qnGlobalSettings->setSystemName(systemName);
    }

    virtual void setLocalSystemId(const QnUuid& localSystemId) override
    {
        qnGlobalSettings->setLocalSystemId(localSystemId);
    }

    virtual bool isCloudInstanceChanged() const override
    {
        return !qnGlobalSettings->cloudHost().isEmpty() &&
                qnGlobalSettings->cloudHost() != nx::network::AppInfo::defaultCloudHost();
    }

    virtual bool isConnectedToCloud() const override
    {
        return !qnGlobalSettings->cloudSystemId().isEmpty();
    }

    virtual bool isSystemIdFromSystemName() const override
    {
        return qnServerModule->roSettings()->value("systemIdFromSystemName").toInt() > 0;
    }

    virtual QString getMaxServerKey() const override
    {
        QString serverKey;
        for (const auto server: resourcePool()->getAllServers(Qn::AnyStatus))
            serverKey = qMax(serverKey, server->getAuthKey());

        return serverKey;
    }
};

SettingsProxyPtr createServerSettingsProxy(QnCommonModule* commonModule)
{
    return std::unique_ptr<ServerSettingsProxy>(new ServerSettingsProxy(commonModule));
}

bool needToResetSystem(bool isNewServerInstance, const SettingsProxy* settings)
{
    return isNewServerInstance || settings->localSystemId().isNull() ||
           (settings->isCloudInstanceChanged() && settings->isConnectedToCloud());
}

bool isNewServerInstance(
    const BeforeRestoreDbData& restoreData,
    bool foundOwnServerInDb,
    bool noSetupWizardFlag)
{
    if (foundOwnServerInDb || noSetupWizardFlag)
        return false;

    return restoreData.localSystemId.isNull();
}

bool setUpSystemIdentity(
        const BeforeRestoreDbData& restoreData,
        SettingsProxy* settings,
        SystemNameProxyPtr systemNameProxy)
{
    LocalSystemIndentityHelper systemIdentityHelper(
            restoreData,
            std::move(systemNameProxy),
            settings);

    if (settings->systemName().isNull())
        settings->setSystemName(systemIdentityHelper.getSystemNameString());
    if (settings->localSystemId().isNull())
        settings->setLocalSystemId(systemIdentityHelper.getLocalSystemId());

    return true;
}

void saveStoragesInfoToBeforeRestoreData(
    BeforeRestoreDbData* beforeRestoreDbData,
    const QnStorageResourceList& storages)
{
    QByteArray result;
    for (const auto& storage : storages)
    {
        result.append(storage->getUrl().toLocal8Bit());
        result.append(";");
        result.append(QByteArray::number(storage->getSpaceLimit()));
        result.append(";");
    }

    beforeRestoreDbData->storageInfo = result;
}

BeforeRestoreDbData savePersistentDataBeforeDbRestore(
        const QnUserResourcePtr& admin,
        const QnMediaServerResourcePtr& mediaServer,
        SettingsProxy* settingsProxy)
{
    BeforeRestoreDbData data;

    NX_ASSERT(admin);
    NX_ASSERT(mediaServer);

    if (admin && admin->isEnabled())
    {
        data.digest = admin->getDigest();
        data.hash = admin->getHash();
        data.cryptSha512Hash = admin->getCryptSha512Hash();
        data.realm = admin->getRealm().toUtf8();
    }

    data.localSystemId = settingsProxy->localSystemId().toByteArray();
    data.localSystemName = settingsProxy->systemName().toLocal8Bit();
    if (mediaServer)
        data.serverName = mediaServer->getName().toLocal8Bit();

    saveStoragesInfoToBeforeRestoreData(&data, mediaServer->getStorages());

    return data;
}

void makeFakeData(const QString& fakeDataString, 
    const ec2::AbstractECConnectionPtr& connection, const QnUuid& serverId)
{
    if (fakeDataString.isEmpty())
        return;

    const auto fakeData = fakeDataString.split(',');
    int userCount = fakeData.value(0).toInt(0);
    int camerasCount = fakeData.value(1).toInt(0);
    int propertiesPerCamera = fakeData.value(2).toInt(0);
    int camerasPerLayout = fakeData.value(3).toInt(0);
    int storageCount = fakeData.value(4).toInt(0);

    qWarning() << "Create fake data:"
        << userCount << "users,"
        << camerasCount << "cameras," << propertiesPerCamera << "properties per camera,"
        << camerasPerLayout << "cameras per layout," << storageCount << "storages";

    std::vector<ec2::ApiUserData> users;
    for (int i = 0; i < userCount; ++i)
    {
        ec2::ApiUserData userData;
        userData.id = QnUuid::createUuid();
        userData.name = lm("user_%1").arg(i);
        userData.isEnabled = true;
        userData.isCloud = false;
        users.push_back(userData);
    }

    std::vector<ec2::ApiCameraData> cameras;
    std::vector<ec2::ApiCameraAttributesData> userAttrs;
    ec2::ApiResourceParamWithRefDataList cameraParams;
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    NX_ASSERT(!resTypePtr.isNull());
    for (int i = 0; i < camerasCount; ++i)
    {
        ec2::ApiCameraData cameraData;
        cameraData.typeId = resTypePtr->getId();
        cameraData.parentId = serverId;
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QnUuid::createUuid().toString();
        cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);
        cameraData.name = lm("Camera %1").arg(cameraData.id);
        cameras.push_back(std::move(cameraData));

        ec2::ApiCameraAttributesData userAttr;
        userAttr.cameraId = cameraData.id;
        userAttrs.push_back(userAttr);

        for (int j = 0; j < propertiesPerCamera; ++j)
        {
            cameraParams.push_back(ec2::ApiResourceParamWithRefData(
                cameraData.id, lit("property%1").arg(j), lit("value%1").arg(j)));
        }
    }

    std::vector<ec2::ApiLayoutData> layouts;
    if (camerasPerLayout)
    {
        for (int minCameraOnLayout = 0; minCameraOnLayout < camerasCount;
             minCameraOnLayout += camerasPerLayout)
        {
            ec2::ApiLayoutData layout;
            layout.id = QnUuid::createUuid();
            for (int cameraIndex = minCameraOnLayout;
                 cameraIndex < minCameraOnLayout + camerasPerLayout && cameraIndex < camerasCount;
                 ++cameraIndex)
            {
                ec2::ApiLayoutItemData item;
                item.id = cameras[cameraIndex].id;
                layout.items.push_back(item);
            }

            layouts.push_back(layout);
        }
    }

    std::vector<ec2::ApiStorageData> storages;
    for (int i = 0; i < storageCount; ++i)
    {
        ec2::ApiStorageData storage;
        storage.id = QnUuid::createUuid();
        storage.parentId = serverId;
        storage.name = lm("Fake Storage/%1").arg(storage.id);
        storage.url = lm("/tmp/fakeStorage/%1").arg(storage.id);
        storages.push_back(storage);
    }

    auto userManager = connection->getUserManager(Qn::kSystemAccess);
    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
    auto layoutManager = connection->getLayoutManager(Qn::kSystemAccess);
    auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);

    for (const auto& user: users)
        NX_ASSERT(ec2::ErrorCode::ok == userManager->saveSync(user));

    NX_ASSERT(ec2::ErrorCode::ok == cameraManager->saveUserAttributesSync(userAttrs));
    NX_ASSERT(ec2::ErrorCode::ok == resourceManager->saveSync(cameraParams));
    NX_ASSERT(ec2::ErrorCode::ok == cameraManager->addCamerasSync(cameras));
    NX_ASSERT(ec2::ErrorCode::ok == serverManager->saveStoragesSync(storages));

    for (const auto& layout: layouts)
        NX_ASSERT(ec2::ErrorCode::ok == layoutManager->saveSync(layout));
}

} // namespace mserver_aux
} // namespace nx


