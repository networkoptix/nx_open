#pragma once

#include <common/common_module.h>

namespace nx {
namespace mserver_aux {

void saveStoragesInfoToBeforeRestoreData(
    BeforeRestoreDbData* beforeRestoreDbData,
    const QnStorageResourceList& storages);

class UnmountedStoragesFilter
{
public:
    UnmountedStoragesFilter(const QString& mediaFolderName);
    QnStorageResourceList getUnmountedStorages(const QnStorageResourceList& allStorages, const QStringList& paths);

private:
    QString getStorageUrlWithoutMediaFolder(const QString& url);

    QString m_mediaFolderName;
};

class SystemNameProxy
{
public:
    virtual void loadFromConfig() = 0;
    virtual void clearFromConfig() = 0;
    virtual void resetToDefault() = 0;
    virtual bool isDefault() const = 0;
    virtual QString value() const = 0;

    virtual ~SystemNameProxy() {}
};

using SystemNameProxyPtr = std::unique_ptr<SystemNameProxy>;

SystemNameProxyPtr createServerSystemNameProxy();

class LocalSystemIndentityHelper
{
public:
    LocalSystemIndentityHelper(
            const BeforeRestoreDbData& restoreData,
            SystemNameProxyPtr systemName);

    QString getSystemNameString() const;
    QString getDefaultSystemNameString() const;
    QnUuid getLocalSystemId() const;
    void clearMigrationInfo();

private:
    QString generateSystemName() const;
    QnUuid generateLocalSystemId() const;

private:
    const BeforeRestoreDbData& m_restoreData;
    SystemNameProxyPtr m_systemName;
};

class SettingsProxy
{
public:
    virtual QString systemName() const = 0;
    virtual QnUuid localSystemId() const = 0;
    virtual void setSystemName(const QString& systemName) = 0;
    virtual void setLocalSystemId(const QnUuid& localSystemId) = 0;
    virtual bool isCloudInstanceChanged() const = 0;
    virtual bool isConnectedToCloud() const = 0;

    virtual ~SettingsProxy() {}
};

using SettingsProxyPtr = std::unique_ptr<SettingsProxy>;
SettingsProxyPtr createServerSettingsProxy();

bool needToResetSystem(bool isNewServerInstance, const SettingsProxy* settings);

bool isNewServerInstance(const BeforeRestoreDbData& restoreData, bool foundOwnServerInDb);

BeforeRestoreDbData savePersistentDataBeforeDbRestore(
        const QnUserResourcePtr& admin,
        const QnMediaServerResourcePtr& mediaServer,
        SettingsProxy* settingsProxy);

bool setUpSystemIdentity(
        const BeforeRestoreDbData& restoreData,
        SettingsProxy* settings,
        SystemNameProxyPtr systemNameProxy);
}
}

