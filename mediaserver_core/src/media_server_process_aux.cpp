#include <media_server/serverutil.h>
#include <api/global_settings.h>
#include "media_server_process_aux.h"
#include <nx/utils/log/log.h>
#include <media_server/settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>

namespace nx {
namespace mserver_aux {

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

class ServerSettingsProxy : public SettingsProxy
{
public:
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
                qnGlobalSettings->cloudHost() != QnAppInfo::defaultCloudHost();
    }

    virtual bool isConnectedToCloud() const override
    {
        return !qnGlobalSettings->cloudSystemId().isEmpty();
    }

    virtual bool isSystemIdFromSystemName() const override
    {
        return MSSettings::roSettings()->value("systemIdFromSystemName").toInt() > 0;
    }

    virtual QString getMaxServerKey() const override
    {
        QString serverKey;
        for (const auto server: qnResPool->getAllServers(Qn::AnyStatus))
            serverKey = qMax(serverKey, server->getAuthKey());

        return serverKey;
    }
};

SettingsProxyPtr createServerSettingsProxy()
{
    return std::unique_ptr<ServerSettingsProxy>(new ServerSettingsProxy);
}

bool needToResetSystem(bool isNewServerInstance, const SettingsProxy* settings)
{
    return isNewServerInstance ||
           (settings->isCloudInstanceChanged() && settings->isConnectedToCloud());
}

bool isNewServerInstance(
    const BeforeRestoreDbData& restoreData, 
    bool foundOwnServerInDb,
    bool noSetupWizardFlag)
{
    if (foundOwnServerInDb)
        return false;

    return noSetupWizardFlag || restoreData.localSystemId.isNull();
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

} // namespace aux
} // namespace nx


