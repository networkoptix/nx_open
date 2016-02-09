
#include "statistics_settings_watcher.h"

#include <QtCore/QTimer>

#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <common/common_module.h>
#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>

QnStatisticsSettingsWatcher::QnStatisticsSettingsWatcher(QObject *parent)
    : base_type(parent)
    , m_settings()
    , m_updateTimer(new QTimer())
    , m_connection()
{
    enum { kUpdatePeriodMs = 30 * 60 * 1000 };    // 30 minutes update period
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(kUpdatePeriodMs);
    m_updateTimer->start();

    connect(m_updateTimer, &QTimer::timeout
        , this, &QnStatisticsSettingsWatcher::updateSettings);

    updateSettings();
}

QnStatisticsSettingsWatcher::~QnStatisticsSettingsWatcher()
{}

bool QnStatisticsSettingsWatcher::settingsAvailable()
{
    if (!m_settings)
        updateSettings();

    return !!m_settings;
}

QnStatisticsSettings QnStatisticsSettingsWatcher::settings()
{
    return (m_settings ? *m_settings : QnStatisticsSettings());
}

void QnStatisticsSettingsWatcher::updateSettings()
{
    if (m_connection)
        return;

    const auto server = qnCommon->currentServer();
    if (!server)
        return;

    m_connection = server->restConnection();

    const auto callback = [this](bool success, rest::Handle handle, const QByteArray &data)
    {
        const auto connectionLifeHolder = m_connection;
        m_connection.reset();

        if (!success)
            return;

        bool deserialized = false;
        const auto settings = QJson::deserialized(data, QnStatisticsSettings(), &deserialized);
        if (!deserialized)
            return;

        m_settings.reset(new QnStatisticsSettings(settings));
        emit settingsAvailableChanged();
    };

    m_connection->getStatisticsSettingsAsync(callback);
}

