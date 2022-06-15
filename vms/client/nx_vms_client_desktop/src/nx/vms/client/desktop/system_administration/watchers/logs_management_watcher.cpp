// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_watcher.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <api/server_rest_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <ui/workbench/workbench_context.h>



#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>
#include <utils/common/delayed.h>

namespace {

} // anonymous namespace

namespace nx::vms::client::desktop {

LogsManagementUnitPtr LogsManagementUnit::createClientUnit()
{
    return std::make_shared<LogsManagementUnit>();
}

LogsManagementUnitPtr LogsManagementUnit::createServerUnit(QnMediaServerResourcePtr server)
{
    auto unit = std::make_shared<LogsManagementUnit>();
    unit->m_server = server;
    return unit;
}

QnUuid LogsManagementUnit::id() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_server ? m_server->getId() : QnUuid();
}

QnMediaServerResourcePtr LogsManagementUnit::server() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_server;
}

bool LogsManagementUnit::isChecked() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_checked;
}

void LogsManagementUnit::setChecked(bool isChecked)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_checked = isChecked;
}

LogsManagementUnit::DownloadState LogsManagementUnit::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

void LogsManagementUnit::setState(LogsManagementUnit::DownloadState state)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_state = state;
}

std::optional<nx::vms::api::ServerLogSettings> LogsManagementUnit::settings() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_settings;
}

void LogsManagementUnit::setSettings(
    const std::optional<nx::vms::api::ServerLogSettings>& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_settings = settings;
}

//-------------------------------------------------------------------------------------------------

LogsManagementWatcher::LogsManagementWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    // Init dummy data.
    m_client = LogsManagementUnit::createClientUnit();

    nx::vms::api::ServerLogSettings settings;
    settings.mainLog.primaryLevel = nx::utils::log::mainLogger()->defaultLevel();
    m_client->setSettings(settings);

    for (const auto& server: resourcePool()->servers())
    {
        m_servers.push_back(LogsManagementUnit::createServerUnit(server));
    }

    loadInitialSettings(); //< TODO: call it when connection is established.

    m_state = State::empty;
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::items() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QList<LogsManagementUnitPtr> result;
    
    result.append(m_client);
    result.append(m_servers);
    
    return result;
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::checkedItems() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QList<LogsManagementUnitPtr> result;

    if (m_client->isChecked())
        result << m_client;

    for (auto server: m_servers)
    {
        if (server->isChecked())
            result << server;
    }

    return result;
}

QString LogsManagementWatcher::path() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_path;
}

void LogsManagementWatcher::setItemIsChecked(LogsManagementUnitPtr item, bool checked)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::empty || m_state == State::hasSelection);

    if (!NX_ASSERT(item))
        return;

    // Update item state.
    item->setChecked(checked);

    // Check if there are selected items.
    bool hasCheckedItems = m_client->isChecked();
    for (auto server: m_servers)
    {
        hasCheckedItems |= server->isChecked();
    }

    auto newState = hasCheckedItems ? State::hasSelection : State::empty;

    if (m_state != newState)
    {
        m_state = newState;

        lock.unlock();
        emit stateChanged(newState);
    }
}

void LogsManagementWatcher::startDownload(const QString& path)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::hasSelection);
    const auto newState = State::loading;

    m_path = path;

    if (m_client->isChecked())
        downloadClientLogs(m_client);

    for (auto server: m_servers)
    {
        if (server->isChecked())
            downloadServerLogs(server);
    }

    m_state = newState;
    
    lock.unlock();
    emit stateChanged(newState);
    emit progressChanged(0);
}

void LogsManagementWatcher::cancelDownload()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::loading);
    const auto newState = State::hasSelection;

    // TODO: stop active downloads.
    m_client->setState(LogsManagementUnit::DownloadState::none);
    for (auto server: m_servers)
    {
        server->setState(LogsManagementUnit::DownloadState::none);
    }

    m_path = "";
    m_state = newState;
    
    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::restartFailed()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::hasErrors);
    const auto newState = State::loading;

    if (m_client->state() == LogsManagementUnit::DownloadState::error)
        downloadClientLogs(m_client);
        
    for (auto server: m_servers)
    {
        if (server->state() == LogsManagementUnit::DownloadState::error)
            downloadServerLogs(m_client);
    }

    m_state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::completeDownload()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::finished || m_state == State::hasErrors);
    const auto newState = State::hasSelection;

    // TODO: some clean-up?
    m_client->setState(LogsManagementUnit::DownloadState::none);
    for (auto server: m_servers)
    {
        server->setState(LogsManagementUnit::DownloadState::none);
    }

    m_path = "";
    m_state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::applySettings(const ConfigurableLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(m_state == State::hasSelection);

    if (m_client->isChecked())
    {
        auto existing = m_client->settings();
        if (!existing)
            return; // TODO?

        m_client->setSettings(settings.applyTo(*existing));
        // TODO: actually store.
    }

    for (auto server: m_servers)
    {
        if (server->isChecked())
            storeServerSettings(server, settings);
    }
}

void LogsManagementWatcher::loadInitialSettings()
{
    auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
        {
            if (!success)
                return;

            auto [list, deserializationResult] = nx::reflect::json::deserialize<
                std::map<std::string, nx::vms::api::ServerLogSettings>>(result.toStdString());

            if (!NX_ASSERT(deserializationResult.success))
                return;

            for (const auto& [key, settings]: list)
            {
                onReceivedServerLogSettings(settings.getId(), settings);
            }
        });

    connection()->serverApi()->getRawResult(
        "/rest/v2/servers/*/logSettings",
        {},
        callback,
        thread()
    );
}

void LogsManagementWatcher::loadServerSettings(const QnUuid& serverId)
{
    auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
        {
            if (!success)
                return;

            auto [settings, deserializationResult] = nx::reflect::json::deserialize<
                nx::vms::api::ServerLogSettings>(result.toStdString());

            if (!NX_ASSERT(deserializationResult.success))
                return;

            onReceivedServerLogSettings(settings.getId(), settings);
        });

    connection()->serverApi()->getRawResult(
        QString("/rest/v2/servers/%1/logSettings").arg(serverId.toString()),
        {},
        callback,
        thread()
    );
}

void LogsManagementWatcher::onReceivedServerLogSettings(
    const QnUuid& serverId,
    const nx::vms::api::ServerLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto server: m_servers)
    {
        if (server->id() == serverId)
        {
            server->setSettings(settings);
            return;
        }
    }
}

bool LogsManagementWatcher::storeServerSettings(
    LogsManagementUnitPtr server,
    const ConfigurableLogSettings& settings)
{
    auto existing = server->settings();
    if (!existing)
        return false; // TODO?

    auto oldSettings = *existing;
    auto newSettings = settings.applyTo(*existing);
    server->setSettings(newSettings);

    auto callback = nx::utils::guarded(this,
        [this, server, oldSettings](
            bool success, rest::Handle requestId, rest::ServerConnection::EmptyResponseType result)
        {
            if (!success)
                server->setSettings(oldSettings);

            // TODO: report an error.
        });

    connection()->serverApi()->putEmptyResult(
        QString("/rest/v2/servers/%1/logSettings").arg(server->id().toString()),
        {},
        nx::String(nx::reflect::json::serialize(newSettings)),
        callback,
        thread()
    );

    return true;
}

void LogsManagementWatcher::downloadClientLogs(LogsManagementUnitPtr unit)
{
    unit->setState(LogsManagementUnit::DownloadState::error);

    executeLater([this]{ updateDownloadState(); }, this);
}

void LogsManagementWatcher::downloadServerLogs(LogsManagementUnitPtr unit)
{
    unit->setState(LogsManagementUnit::DownloadState::loading);

    auto callback = nx::utils::guarded(this,
        [this, unit](bool success, rest::Handle requestId, QByteArray result, auto headers)
        {
            unit->setState(success
                ? LogsManagementUnit::DownloadState::complete
                : LogsManagementUnit::DownloadState::error);

            // TODO: a better way to name files.
            QDir dir(m_path);
            QString base = unit->server() ? unit->id().toSimpleString() : "client_log";
            QString filename;
            for (int i = 0;; i++)
            {
                filename = i
                    ? QString("%1 (%2).zip").arg(base).arg(i)
                    : QString("%1.zip").arg(base);

                if (!dir.exists(filename))
                    break;
            }
            QFile file(dir.absoluteFilePath(filename));
            file.open(QIODevice::WriteOnly);
            file.write(result);
            file.close();

            updateDownloadState();
        });

    connection()->serverApi()->getRawResult(
        QString("/rest/v2/servers/%1/logArchive").arg(unit->server()->getId().toString()),
        {},
        callback,
        thread()
    );
}

void LogsManagementWatcher::updateDownloadState()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    int loadingCount = 0, successCount = 0, errorCount = 0;
    auto updateCounters =
        [&](const LogsManagementUnitPtr& unit)
        {
            using State = LogsManagementUnit::DownloadState;
            switch (unit->state())
            {
                case State::loading:
                    loadingCount++;
                    break;
                case State::complete:
                    successCount++;
                    break;
                case State::error:
                    errorCount++;
                    break;
            }
        };

    updateCounters(m_client);
    for (auto server: m_servers)
    {
        updateCounters(server);
    }

    auto newState = loadingCount
        ? State::loading
        : errorCount
            ? State::hasErrors
            : State::finished;

    bool changed = (m_state != newState);
    m_state = newState;

    lock.unlock();
    if (changed)
        emit stateChanged(newState);

    auto completedCount = successCount + errorCount;
    auto totalCount = completedCount + loadingCount;
    emit progressChanged(1. * completedCount / totalCount);
}

} // namespace nx::vms::client::desktop
