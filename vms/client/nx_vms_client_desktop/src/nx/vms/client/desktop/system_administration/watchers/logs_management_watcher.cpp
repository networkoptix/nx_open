// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_watcher.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h> // TODO
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h> // TODO

namespace nx::vms::client::desktop {

namespace {

bool needLogLevelWarningFor(const LogsManagementWatcher::UnitPtr& unit)
{
    auto settings = unit->settings();
    return settings && settings->mainLog.primaryLevel > nx::utils::log::Level::info;
}

QString shortList(const QList<QnMediaServerResourcePtr>& servers)
{
    QString result;

    for (int i = 0; i < qMin(3, servers.size()); ++i)
    {
        auto info = QnResourceDisplayInfo(servers[i]);
        result += nx::format("<b>%1</b> (%2) <br/>", info.name(), info.host());
    }

    if (servers.size() > 3)
        result += LogsManagementWatcher::tr("... and %n more", "", servers.size() - 3);

    return result;
}

} // namespace

struct LogsManagementWatcher::Unit::Private
{
    static std::shared_ptr<Unit> createClientUnit()
    {
        return std::make_shared<Unit>();
    }

    static std::shared_ptr<Unit> createServerUnit(QnMediaServerResourcePtr server)
    {
        auto unit = std::make_shared<LogsManagementWatcher::Unit>();
        unit->d->m_server = server;
        return unit;
    }

    QnUuid id() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_server ? m_server->getId() : QnUuid();
    }

    bool isChecked() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_checked;
    }

    void setChecked(bool isChecked)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_checked = isChecked;
    }

    QnMediaServerResourcePtr server() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_server;
    }

    std::optional<nx::vms::api::ServerLogSettings> settings() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_settings;
    }

    void setSettings(const std::optional<nx::vms::api::ServerLogSettings>& settings)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_settings = settings;
    }

    DownloadState state() const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_state;
    }

    void setState(Unit::DownloadState state)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_state = state;
    }

private:
    mutable nx::Mutex m_mutex;
    QnMediaServerResourcePtr m_server;

    bool m_checked{false};
    DownloadState m_state{DownloadState::none};
    std::optional<nx::vms::api::ServerLogSettings> m_settings;
};

QnUuid LogsManagementWatcher::Unit::id() const
{
    return d->id();
}

bool LogsManagementWatcher::Unit::isChecked() const
{
    return d->isChecked();
}

QnMediaServerResourcePtr LogsManagementWatcher::Unit::server() const
{
    return d->server();
}

std::optional<nx::vms::api::ServerLogSettings> LogsManagementWatcher::Unit::settings() const
{
    return d->settings();
}

LogsManagementWatcher::Unit::DownloadState LogsManagementWatcher::Unit::state() const
{
    return d->state();
}

LogsManagementWatcher::Unit::Private* LogsManagementWatcher::Unit::data()
{
    return d.get();
}

LogsManagementWatcher::Unit::Unit():
    d(new Private())
{
}

//-------------------------------------------------------------------------------------------------

struct LogsManagementWatcher::Private
{
    Private(LogsManagementWatcher* q): q(q), api(new Api(q))
    {
    }

    mutable nx::Mutex mutex;
    LogsManagementWatcher* const q;

    QString path;
    State state{State::empty};
    bool updatesEnabled{false};

    LogsManagementUnitPtr client;
    QList<LogsManagementUnitPtr> servers;

    QPointer<workbench::LocalNotificationsManager> notificationManager;
    QnUuid clientLogLevelWarning;
    QnUuid serverLogLevelWarning;

    QList<LogsManagementUnitPtr> items() const
    {
        QList<LogsManagementUnitPtr> result;
        result << client;
        result << servers;
        return result;
    }

    QList<LogsManagementUnitPtr> checkedItems() const
    {
        QList<LogsManagementUnitPtr> result;

        if (client->isChecked())
            result << client;

        for (auto& server: servers)
        {
            if (server->isChecked())
                result << server;
        }

        return result;
    }

    State selectionState() const
    {
        bool isChecked = client->isChecked();
        for (auto& server: servers)
        {
            isChecked |= server->isChecked();
        }
        return isChecked ? State::hasSelection : State::empty;
    }

    void initNotificationManager()
    {
        if (!notificationManager)
        {
            notificationManager = appContext()->mainWindowContext()->workbenchContext()
                ->instance<workbench::LocalNotificationsManager>();

            q->connect(notificationManager, &workbench::LocalNotificationsManager::cancelRequested, q,
                [this](const QnUuid& notificationId)
                {
                    NX_MUTEX_LOCKER lock(&mutex);
                    hideNotification(notificationId);
                });
        }
    }

    void updateClientLogLevelWarning()
    {
        if (!NX_ASSERT(notificationManager))
            return;

        if (needLogLevelWarningFor(client))
        {
            if (clientLogLevelWarning.isNull())
            {
                clientLogLevelWarning = notificationManager->add(
                    tr("Debug Logging is enabled on Client"),
                    {},
                    true);
                notificationManager->setLevel(clientLogLevelWarning,
                    QnNotificationLevel::Value::ImportantNotification);
            }
        }
        else
        {
            if (clientLogLevelWarning.isNull())
                return;

            notificationManager->remove(clientLogLevelWarning);
            clientLogLevelWarning = {};
        }
    }

    void updateServerLogLevelWarning()
    {
        if (!NX_ASSERT(notificationManager))
            return;

        QList<QnMediaServerResourcePtr> resList;
        for (auto& s: servers)
        {
            if (needLogLevelWarningFor(s))
                resList << s->server();
        }

        if (!resList.isEmpty())
        {
            if (serverLogLevelWarning.isNull())
            {
                serverLogLevelWarning = notificationManager->add({}, {}, true);
                notificationManager->setLevel(serverLogLevelWarning,
                    QnNotificationLevel::Value::ImportantNotification);
            }

            notificationManager->setTitle(serverLogLevelWarning,
                tr("Debug Logging is enabled on %n Servers", "", resList.size()));

            notificationManager->setAdditionalText(serverLogLevelWarning, shortList(resList));
        }
        else
        {
            if (serverLogLevelWarning.isNull())
                return;

            notificationManager->remove(serverLogLevelWarning);
            serverLogLevelWarning = {};
        }
    }

    void hideNotification(const QnUuid& notificationId)
    {
        if (notificationId == clientLogLevelWarning)
        {
            notificationManager->remove(clientLogLevelWarning);
            clientLogLevelWarning = {};
        }
        else if (notificationId == serverLogLevelWarning)
        {
            notificationManager->remove(serverLogLevelWarning);
            serverLogLevelWarning = {};
        }
    }

    // A helper struct with long-running functions that should be called without locking the mutex.
    // These functions are encapsulated into a separate struct to ensure that they don't access any
    // Private fields.
    struct Api
    {
        Api(LogsManagementWatcher* q): q(q) {}
        LogsManagementWatcher* const q;

        void loadInitialSettings()
        {
            auto callback = nx::utils::guarded(q,
                [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
                {
                    if (!success)
                        return;

                    auto [list, deserializationResult] = nx::reflect::json::deserialize<
                        std::map<std::string, nx::vms::api::ServerLogSettings>>(
                            result.toStdString());

                    if (!NX_ASSERT(deserializationResult.success))
                        return;

                    for (const auto& [key, settings]: list)
                    {
                        q->onReceivedServerLogSettings(settings.getId(), settings);
                    }
                });

            q->connection()->serverApi()->getRawResult(
                "/rest/v2/servers/*/logSettings",
                {},
                callback,
                q->thread()
            );
        }

        void loadServerSettings(const QnUuid& serverId)
        {
            auto callback = nx::utils::guarded(q,
                [this](bool success, rest::Handle requestId, QByteArray result, auto headers)
                {
                    if (!success)
                        return;

                    auto [settings, deserializationResult] = nx::reflect::json::deserialize<
                        nx::vms::api::ServerLogSettings>(result.toStdString());

                    if (!NX_ASSERT(deserializationResult.success))
                        return;

                    q->onReceivedServerLogSettings(settings.getId(), settings);
                });

            q->connection()->serverApi()->getRawResult(
                QString("/rest/v2/servers/%1/logSettings").arg(serverId.toString()),
                {},
                callback,
                q->thread()
            );
        }

        bool storeServerSettings(
            LogsManagementUnitPtr server,
            const ConfigurableLogSettings& settings)
        {
            auto existing = server->settings();
            if (!existing)
                return false; // TODO: report an error.

            auto oldSettings = *existing;
            auto newSettings = settings.applyTo(*existing);
            server->data()->setSettings(newSettings);

            auto callback = nx::utils::guarded(q,
                [this, server, oldSettings](
                    bool success,
                    rest::Handle requestId,
                    rest::ServerConnection::EmptyResponseType result)
                {
                    if (!success)
                        server->data()->setSettings(oldSettings);

                    // TODO: report an error.
                });

            q->connection()->serverApi()->putEmptyResult(
                QString("/rest/v2/servers/%1/logSettings").arg(server->id().toString()),
                {},
                nx::String(nx::reflect::json::serialize(newSettings)),
                callback,
                q->thread()
            );

            return true;
        }
    };
    QScopedPointer<Api> api;
};

LogsManagementWatcher::LogsManagementWatcher(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private(this))
{
    d->client = Unit::Private::createClientUnit();
    nx::vms::api::ServerLogSettings settings;
    settings.mainLog.primaryLevel = nx::utils::log::mainLogger()->defaultLevel();
    d->client->data()->setSettings(settings);

    connect(
        messageProcessor(),
        &QnCommonMessageProcessor::initialResourcesReceived,
        this,
        [this]
        {
            {
                NX_MUTEX_LOCKER lock(&d->mutex);
                d->path = "";
                d->state = State::empty;

                d->initNotificationManager();
                d->updateClientLogLevelWarning();
                d->updateServerLogLevelWarning();
            }

            d->api->loadInitialSettings();
        });

    connect(resourcePool(), &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            QList<QnUuid> addedServers;

            {
                NX_MUTEX_LOCKER lock(&d->mutex);

                for (const auto& resource: resources)
                {
                    if (auto server = resource.dynamicCast<QnMediaServerResource>();
                        server && !server->hasFlags(Qn::fake_server))
                    {
                        d->servers << Unit::Private::createServerUnit(server);
                        addedServers << server->getId();
                    }
                }
            }

            if (d->updatesEnabled)
            {
                for (auto& serverId: addedServers)
                    d->api->loadServerSettings(serverId);
            }

            if (!addedServers.isEmpty())
                emit itemListChanged();
        });

    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            bool removed = false;

            {
                NX_MUTEX_LOCKER lock(&d->mutex);

                for (const auto& resource: resources)
                {
                    if (auto server = resource.dynamicCast<QnMediaServerResource>())
                    {

                        auto it = std::find_if(d->servers.begin(), d->servers.end(),
                            [id=server->getId()](auto& unit){ return unit->id() == id; });

                        if (it == d->servers.end())
                            continue;

                        d->servers.erase(it);
                        removed = true;
                    }
                }
            }

            if (removed)
                emit itemListChanged();
        });
}

LogsManagementWatcher::~LogsManagementWatcher()
{
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::items() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->items();
}

QList<LogsManagementUnitPtr> LogsManagementWatcher::checkedItems() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->checkedItems();
}

QString LogsManagementWatcher::path() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->path;
}

void LogsManagementWatcher::setItemIsChecked(LogsManagementUnitPtr item, bool checked)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::empty || d->state == State::hasSelection);

    if (!NX_ASSERT(item))
        return;

    // Update item state.
    item->data()->setChecked(checked);

    // Evaluate the new state.
    auto newState = d->selectionState();
    if (d->state != newState)
    {
        d->state = newState;

        lock.unlock();
        emit stateChanged(newState);
    }
}

void LogsManagementWatcher::startDownload(const QString& path)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasSelection);
    const auto newState = State::loading;

    d->path = path;

    if (d->client->isChecked())
        downloadClientLogs(d->client);

    for (auto server: d->servers)
    {
        if (server->isChecked())
            downloadServerLogs(server); // TODO: unlock.
    }

    d->state = newState;

    lock.unlock();
    emit stateChanged(newState);
    emit progressChanged(0);
}

void LogsManagementWatcher::cancelDownload()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::loading);
    const auto newState = d->selectionState();

    // TODO: stop active downloads.
    d->client->data()->setState(Unit::DownloadState::none);
    for (auto server: d->servers)
    {
        server->data()->setState(Unit::DownloadState::none);
    }

    d->path = "";
    d->state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::restartFailed()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasErrors);
    const auto newState = State::loading;

    if (d->client->state() == Unit::DownloadState::error)
        downloadClientLogs(d->client); // TODO: unlock.

    for (auto server: d->servers)
    {
        if (server->state() == Unit::DownloadState::error)
            downloadServerLogs(d->client);
    }

    d->state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::completeDownload()
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::finished || d->state == State::hasErrors);
    const auto newState = d->selectionState();

    // TODO: some clean-up?
    d->client->data()->setState(Unit::DownloadState::none);
    for (auto server: d->servers)
    {
        server->data()->setState(Unit::DownloadState::none);
    }

    d->path = "";
    d->state = newState;

    lock.unlock();
    emit stateChanged(newState);
}

void LogsManagementWatcher::setUpdatesEnabled(bool enabled)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->updatesEnabled = enabled;
    }

    if (enabled)
        d->api->loadInitialSettings();
}

void LogsManagementWatcher::applySettings(const ConfigurableLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    NX_ASSERT(d->state == State::hasSelection);

    if (d->client->isChecked())
    {
        auto existing = d->client->settings();
        if (!NX_ASSERT(existing))
            return;

        d->client->data()->setSettings(settings.applyTo(*existing));
        // TODO: actually store.
    }

    for (auto server: d->servers)
    {
        if (server->isChecked())
            d->api->storeServerSettings(server, settings); // TODO: unlock.
    }
    d->updateClientLogLevelWarning();
    d->updateServerLogLevelWarning();
}

void LogsManagementWatcher::onReceivedServerLogSettings(
    const QnUuid& serverId,
    const nx::vms::api::ServerLogSettings& settings)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    for (auto server: d->servers)
    {
        if (server->id() == serverId)
        {
            server->data()->setSettings(settings);
            if (needLogLevelWarningFor(server))
                d->updateServerLogLevelWarning();
            return;
        }
    }
}

void LogsManagementWatcher::downloadClientLogs(LogsManagementUnitPtr unit)
{
    unit->data()->setState(Unit::DownloadState::error);

    executeLater([this]{ updateDownloadState(); }, this);
}

void LogsManagementWatcher::downloadServerLogs(LogsManagementUnitPtr unit)
{
    unit->data()->setState(Unit::DownloadState::loading);

    auto callback = nx::utils::guarded(this,
        [this, unit](bool success, rest::Handle requestId, QByteArray result, auto headers)
        {
            unit->data()->setState(success
                ? Unit::DownloadState::complete
                : Unit::DownloadState::error);

            // TODO: a better way to name files.
            QDir dir(d->path);
            QString base = unit->server() ? unit->id().toSimpleString() : "client_log";
            QString filename;
            for (int i = 0;; i++)
            {
                filename = i
                    ? nx::format("%1 (%2).zip", base, i)
                    : nx::format("%1.zip", base);

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
    NX_MUTEX_LOCKER lock(&d->mutex);

    int loadingCount = 0, successCount = 0, errorCount = 0;
    auto updateCounters =
        [&](const LogsManagementUnitPtr& unit)
        {
            using State = Unit::DownloadState;
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

    updateCounters(d->client);
    for (auto server: d->servers)
    {
        updateCounters(server);
    }

    auto newState = loadingCount
        ? State::loading
        : errorCount
            ? State::hasErrors
            : State::finished;

    bool changed = (d->state != newState);
    d->state = newState;

    lock.unlock();
    if (changed)
        emit stateChanged(newState);

    auto completedCount = successCount + errorCount;
    auto totalCount = completedCount + loadingCount;
    emit progressChanged(1. * completedCount / totalCount);
}

} // namespace nx::vms::client::desktop
