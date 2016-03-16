
#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <core/system_connection_data.h>

class QSettings;

class QnCoreSettings : public QnPropertyStorage
    , public Singleton<QnCoreSettings>
{
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum PropertyIdentifier
    {
        LastSystemConnections

        , PropertiesCount
    };

public:
    QnCoreSettings(QObject *parent = nullptr);

    virtual ~QnCoreSettings();

private:
    QN_BEGIN_PROPERTY_STORAGE(PropertiesCount)
        QN_DECLARE_RW_PROPERTY(QnSystemConnectionDataList, lastSystemConnections
            , setLastSystemConnections, LastSystemConnections, QnSystemConnectionDataList())
    QN_END_PROPERTY_STORAGE()

private:
    typedef QScopedPointer<QSettings> SettingsPtr;
    const SettingsPtr m_settings;
};

#define qnCoreSettings QnCoreSettings::instance()