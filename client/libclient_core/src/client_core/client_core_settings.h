#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <client_core/local_connection_data.h>
#include <watchers/cloud_status_watcher.h>

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
        CloudLogin,
        CloudPassword,
        RecentCloudSystems,

        PropertiesCount
    };

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
            const QVariant& defaultValue) override;

    void save();

private:
    QN_BEGIN_PROPERTY_STORAGE(PropertiesCount)
        QN_DECLARE_RW_PROPERTY(QList<QnLocalConnectionData>,
            recentLocalConnections, setRecentLocalConnections,
            RecentLocalConnections, QList<QnLocalConnectionData>())
        QN_DECLARE_RW_PROPERTY(QList<QnLocalConnectionWeightData>,
            localSystemWeightsData, setLocalSystemWeightsData,
            LocalSystemWeightsData, QList<QnLocalConnectionWeightData>())
        QN_DECLARE_RW_PROPERTY(QString,
            cdbEndpoint, setCdbEndpoint,
            CdbEndpoint, QString())
        QN_DECLARE_RW_PROPERTY(QString,
            cloudLogin, setCloudLogin,
            CloudLogin, QString())
        QN_DECLARE_RW_PROPERTY(QString,
            cloudPassword, setCloudPassword,
            CloudPassword, QString())
        QN_DECLARE_RW_PROPERTY(QnCloudSystemList,
            recentCloudSystems, setRecentCloudSystems,
            RecentCloudSystems, QnCloudSystemList())
        QN_END_PROPERTY_STORAGE()

private:
    QSettings* m_settings;
};

#define qnClientCoreSettings QnClientCoreSettings::instance()
