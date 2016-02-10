
#include "statistics_settings_watcher.h"

#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <common/common_module.h>
#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>

QnStatisticsSettingsWatcher::QnStatisticsSettingsWatcher(QObject *parent)
    : base_type(parent)
    , m_settings()
    , m_updateTimer(new QTimer())
    , m_handle()
{
    enum { kUpdatePeriodMs = 30 * 60 * 1000 };
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

    return m_settings;
}

QnStatisticsSettings QnStatisticsSettingsWatcher::settings()
{
    return (m_settings ? *m_settings : QnStatisticsSettings());
}

void QnStatisticsSettingsWatcher::updateSettings()
{
    enum { kImmediately = 0 };
    updateSettingsImpl(kImmediately);
}

void QnStatisticsSettingsWatcher::updateSettingsImpl(int delayMs)
{
    const bool correctDelay = (delayMs >= 0);
    Q_ASSERT_X(correctDelay, Q_FUNC_INFO, "Delay could not be less than 0!");

    if (!correctDelay || m_handle)
        return;

    const QPointer<QnStatisticsSettingsWatcher> thisGuard(this);
    const auto callback = [this, thisGuard]
        (bool success, rest::Handle handle, const QByteArray &data)
    {
        if (!thisGuard)
            return;

        if (handle != m_handle)
            return;

        m_handle = rest::Handle();
        enum { kUpdateOnFailDelayMs = 5 * 60 * 1000 };
        if (!success)
        {
            updateSettingsImpl(kUpdateOnFailDelayMs);
            return;
        }

        bool deserialized = false;
        const auto settings = QJson::deserialized(data, QnStatisticsSettings(), &deserialized);
        if (!deserialized)
        {
            updateSettingsImpl(kUpdateOnFailDelayMs);
            return;
        }

        m_settings.reset(new QnStatisticsSettings(settings));
        emit settingsAvailableChanged();
    };

    const auto getStatSettingsHandler = [this, callback, thisGuard]()
    {
        if (!thisGuard)
            return;

        const auto server = qnCommon->currentServer();
        if (!server)
            return;

        const auto connection = server->restConnection();
        if (!connection)
            return;

        m_handle = connection->getStatisticsSettingsAsync(
            callback, QThread::currentThread());
    };

    if (!delayMs)
        getStatSettingsHandler();
    else
        executeDelayedParented(getStatSettingsHandler, delayMs, this);
}

