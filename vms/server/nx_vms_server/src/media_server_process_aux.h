#pragma once

#include <common/common_module.h>
#include <nx_ec/ec_api_fwd.h>
#include <platform/platform_abstraction.h>

class QnMediaServerModule;

namespace nx {

namespace vms::server { class Settings; }

namespace mserver_aux {

void saveStoragesInfoToBeforeRestoreData(
    BeforeRestoreDbData* beforeRestoreDbData,
    const QnStorageResourceList& storages);

bool isPathMounted(const QString& path, const QStringList& mediaPaths);

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

SystemNameProxyPtr createServerSystemNameProxy(QnMediaServerModule* serverModule);

class SettingsProxy
{
public:
    virtual QString systemName() const = 0;
    virtual QnUuid localSystemId() const = 0;
    virtual void setSystemName(const QString& systemName) = 0;
    virtual void setLocalSystemId(const QnUuid& localSystemId) = 0;
    virtual bool isCloudInstanceChanged() const = 0;
    virtual bool isConnectedToCloud() const = 0;
    virtual bool isSystemIdFromSystemName() const = 0;
    virtual QString getMaxServerKey() const = 0;

    virtual ~SettingsProxy() {}
};

using SettingsProxyPtr = std::unique_ptr<SettingsProxy>;
SettingsProxyPtr createServerSettingsProxy(QnMediaServerModule* serverModule);


class LocalSystemIndentityHelper
{
public:
    LocalSystemIndentityHelper(
            const BeforeRestoreDbData& restoreData,
            SystemNameProxyPtr systemName,
            const SettingsProxy* settings);

    QString getSystemNameString() const;
    QnUuid getLocalSystemId() const;

private:
    void initSystemName();
    void initLocalSystemId();
    QnUuid generateLocalSystemId() const;

private:
    const BeforeRestoreDbData& m_restoreData;
    SystemNameProxyPtr m_systemName;
    QString m_systemNameString;
    QnUuid m_localSystemId;
    const SettingsProxy* m_settings;
};

bool needToResetSystem(bool isNewServerInstance, const SettingsProxy* settings);

bool isNewServerInstance(
    const BeforeRestoreDbData& restoreData,
    bool foundOwnServerInDb,
    bool noSetupWizardFlag);

BeforeRestoreDbData savePersistentDataBeforeDbRestore(
        const QnUserResourcePtr& admin,
        const QnMediaServerResourcePtr& mediaServer,
        SettingsProxy* settingsProxy);

bool setUpSystemIdentity(
        const BeforeRestoreDbData& restoreData,
        SettingsProxy* settings,
        SystemNameProxyPtr systemNameProxy);

void makeFakeData(const QString& fakeDataString,
    const ec2::AbstractECConnectionPtr& connection, const QnUuid& moduleGuid);

} // namespace mserver_aux
} // namespace nx

