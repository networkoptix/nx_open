#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <core/user_recent_connection_data.h>

class QSettings;

//TODO: #ynikitenkov move to client_core subdirectory and rename to QnClientCoreSettings
class QnCoreSettings :
    public QnPropertyStorage,
    public Singleton<QnCoreSettings>
{
    Q_OBJECT
    using base_type = QnPropertyStorage;

public:
    enum PropertyIdentifier
    {
        RecentUserConnections,

        PropertiesCount
    };

public:
    QnCoreSettings(QObject* parent = nullptr);

    virtual ~QnCoreSettings();

    virtual void writeValueToSettings(QSettings *settings
        , int id
        , const QVariant &value) const override;

    virtual QVariant readValueFromSettings(QSettings *settings
        , int id
        , const QVariant &defaultValue) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(PropertiesCount)
        QN_DECLARE_RW_PROPERTY(QnUserRecentConnectionDataList, recentUserConnections
            , setRecentUserConnections, RecentUserConnections, QnUserRecentConnectionDataList())
    QN_END_PROPERTY_STORAGE()


private:
    QSettings* m_settings;
};

#define qnCoreSettings QnCoreSettings::instance()
