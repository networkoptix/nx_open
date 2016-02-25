
#include "statistics_settings_watcher.h"

#include <QtCore/QTimer>

#include <nx/network/http/asynchttpclient.h>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>

namespace
{
    enum { kImmediately = 0 };
}

QnStatisticsSettingsWatcher::QnStatisticsSettingsWatcher(QObject *parent)
    : base_type(parent)
    , m_settings()
    , m_updateTimer(new QTimer())
    , m_handle()
{
    enum { kUpdatePeriodMs = 5 * 60 * 1000 };
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
    updateSettingsImpl(kImmediately);
}

void QnStatisticsSettingsWatcher::updateSettingsImpl(int delayMs)
{
    const bool correctDelay = (delayMs >= 0);
    Q_ASSERT_X(correctDelay, Q_FUNC_INFO, "Delay could not be less than 0!");

    if (!correctDelay || m_handle)
        return;

    const QPointer<QnStatisticsSettingsWatcher> guard(this);
    const auto callback = [this, guard]
        (bool success, rest::Handle handle, const QByteArray &data)
    {
        if (!guard)
            return;

        if (handle != m_handle)
            return;

        m_handle = rest::Handle();
        enum { kUpdateOnFailDelayMs = 5 * 60 * 1000 };
        if (!success)
        {
            resetSettings();
            updateSettingsImpl(kUpdateOnFailDelayMs);
            return;
        }

        bool deserialized = false;
        const auto settings = QJson::deserialized(data, QnStatisticsSettings(), &deserialized);
        if (!deserialized)
        {
            resetSettings();
            updateSettingsImpl(kUpdateOnFailDelayMs);
            return;
        }

        setSettings(settings);
    };

    if (delayMs != kImmediately)
    {
        const auto updateSettingsHandler = [this, guard]()
        {
            if (guard)
                updateSettings();
        };

        executeDelayedParented(updateSettingsHandler, delayMs, this);
        return;
    }

    const auto server = qnCommon->currentServer();
    if (!server)
        return;

    const auto connection = server->restConnection();
    if (!connection)
        return;

    m_handle = connection->getStatisticsSettingsAsync(
        callback, QThread::currentThread());
    return;

}

void QnStatisticsSettingsWatcher::resetSettings()
{
    if (!m_settings)
        return;

    m_settings.reset();
    emit settingsAvailableChanged();
}

void QnStatisticsSettingsWatcher::setSettings(const QnStatisticsSettings &settings)
{
    const bool sameSettings = (m_settings && (*m_settings == settings));
    if (sameSettings)
        return;

    m_settings.reset(new QnStatisticsSettings(settings));
    emit settingsAvailableChanged();
}
