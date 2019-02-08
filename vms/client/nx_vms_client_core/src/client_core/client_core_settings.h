#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/url.h>
#include <client_core/local_connection_data.h>
#include <client/forgotten_systems_manager.h>
#include <watchers/cloud_status_watcher.h>
#include <nx/client/core/watchers/known_server_connections.h>
#include <utils/common/credentials.h>
#include <nx/vms/client/core/common/utils/encoded_credentials.h>

class QSettings;

class QnClientCoreSettings :
    public QnPropertyStorage,
    public Singleton<QnClientCoreSettings>
{
    Q_OBJECT
    using base_type = QnPropertyStorage;

public:
    enum PropertyIdentifier
    {
        RecentLocalConnections,
        LocalSystemWeightsData,
        CdbEndpoint,
        RecentCloudSystems,
        ForgottenSystems,
        SkipStartupTilesManagement,
        StartupDiscoveryPeriodMs,
        KnownServerConnections,

        // Obsolete values.
        CloudLogin,
        CloudPassword,
        KnownServerUrls,
        SystemAuthenticationData,

        PropertiesCount
    };

    using RecentLocalConnectionsHash = QHash<QnUuid, nx::vms::client::core::LocalConnectionData>;
    using WeightDataList = QList<nx::vms::client::core::WeightData>;
    using KnownServerConnection = nx::vms::client::core::watchers::KnownServerConnections::Connection;

public:
    QnClientCoreSettings(QObject* parent = nullptr);

    virtual ~QnClientCoreSettings();

    virtual void writeValueToSettings(
            QSettings* settings,
            int id,
            const QVariant& value) const override;

    virtual QVariant readValueFromSettings(
            QSettings* settings,
            int id,
            const QVariant& defaultValue) const override;

    void save();

private:
    QN_BEGIN_PROPERTY_STORAGE(PropertiesCount)
        QN_DECLARE_RW_PROPERTY(RecentLocalConnectionsHash,
           recentLocalConnections, setRecentLocalConnections,
           RecentLocalConnections, RecentLocalConnectionsHash())
        QN_DECLARE_RW_PROPERTY(WeightDataList,
            localSystemWeightsData, setLocalSystemWeightsData,
            LocalSystemWeightsData, WeightDataList())
        QN_DECLARE_RW_PROPERTY(QString,
            cdbEndpoint, setCdbEndpoint,
            CdbEndpoint, QString())
        QN_DECLARE_RW_PROPERTY(QnCloudSystemList,
            recentCloudSystems, setRecentCloudSystems,
            RecentCloudSystems, QnCloudSystemList())
        QN_DECLARE_RW_PROPERTY(QnStringSet,
            forgottenSystems, setForgottenSystems,
            ForgottenSystems, QnStringSet())
        QN_DECLARE_RW_PROPERTY(int,
            startupDiscoveryPeriodMs, setStartupDiscoveryPeriodMs,
            StartupDiscoveryPeriodMs, 2000)
        QN_DECLARE_RW_PROPERTY(bool,
            skipStartupTilesManagement, setSkipStartupTilesManagement,
            SkipStartupTilesManagement, false)
        QN_DECLARE_RW_PROPERTY(QList<KnownServerConnection>,
            knownServerConnections, setKnownServerConnections,
            KnownServerConnections, QList<KnownServerConnection>())

        // Obsolete values.

        using SystemAuthenticationDataHash =
            QHash<QnUuid, QList<nx::vms::client::core::EncodedCredentials>>;
        QN_DECLARE_RW_PROPERTY(QList<nx::utils::Url>,
            knownServerUrls, setKnownServerUrls,
            KnownServerUrls, QList<nx::utils::Url>())
        QN_DECLARE_RW_PROPERTY(SystemAuthenticationDataHash,
            systemAuthenticationData, setSystemAuthenticationData,
            SystemAuthenticationData, SystemAuthenticationDataHash())
        QN_DECLARE_RW_PROPERTY(QString,
            cloudLogin, setCloudLogin,
            CloudLogin, QString())
        QN_DECLARE_RW_PROPERTY(QString,
            cloudPassword, setCloudPassword,
            CloudPassword, QString())
    QN_END_PROPERTY_STORAGE()

private:
    QSettings* m_settings;
};

#define qnClientCoreSettings QnClientCoreSettings::instance()
