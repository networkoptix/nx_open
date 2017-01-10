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
        SystemNameProxyPtr systemName):
    m_restoreData(restoreData),
    m_systemName(std::move(systemName))
{}

QString LocalSystemIndentityHelper::getSystemNameString() const
{
    if (!m_restoreData.localSystemName.isNull())
        return QString::fromLocal8Bit(m_restoreData.localSystemName);

    return generateSystemName();
}

QString LocalSystemIndentityHelper::getDefaultSystemNameString() const
{
    nx::SystemName systemName;
    systemName.resetToDefault();
    return systemName.value();
}

QString LocalSystemIndentityHelper::generateSystemName() const
{
    m_systemName->loadFromConfig();
    if (m_systemName->value().isEmpty())
        return QString();

    return m_systemName->value();
}

QnUuid LocalSystemIndentityHelper::getLocalSystemId() const
{
    if (!m_restoreData.localSystemId.isNull())
        return QnUuid::fromStringSafe(m_restoreData.localSystemId);

    return generateLocalSystemId();
}

QnUuid LocalSystemIndentityHelper::generateLocalSystemId() const
{
    NX_ASSERT(!m_systemName->isDefault());
    if (m_systemName->isDefault())
        NX_LOG(lit("%1 SystemName is default. System state should be reseted."), cl_logWARNING);

    QString serverKey;
    if (!MSSettings::roSettings()->value("systemIdFromSystemName").toInt())
    {
        for (const auto server: qnResPool->getAllServers(Qn::AnyStatus))
            serverKey = qMax(serverKey, server->getAuthKey());
    }
    return guidFromArbitraryData(m_systemName->value() + serverKey);

}

void LocalSystemIndentityHelper::clearMigrationInfo()
{
    m_systemName->clearFromConfig();
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
    LocalSystemIndentityHelper systemIdentityHelper(restoreData, std::move(systemNameProxy));
    if (systemIdentityHelper.getSystemNameString().isEmpty())
    {
        settings->setSystemName(systemIdentityHelper.getDefaultSystemNameString());
        return false;
    }

    settings->setSystemName(systemIdentityHelper.getSystemNameString());
    settings->setLocalSystemId(systemIdentityHelper.getLocalSystemId());

    systemIdentityHelper.clearMigrationInfo();

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


