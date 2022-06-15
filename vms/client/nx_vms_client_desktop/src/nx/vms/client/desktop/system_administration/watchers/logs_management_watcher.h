// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/log_settings.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api { struct ServerLogSettings; }

namespace nx::vms::client::desktop {

struct ConfigurableLogSettings; 

class LogsManagementUnit
{
public:
    enum class DownloadState
    {
        none,
        pending,
        loading,
        complete,
        error,
    };

    QnUuid id() const;
    QnMediaServerResourcePtr server() const;

    bool isChecked() const;

    DownloadState state() const;

    std::optional<nx::vms::api::ServerLogSettings> settings() const;

private:
    friend class LogsManagementWatcher;

    static std::shared_ptr<LogsManagementUnit> createClientUnit();
    static std::shared_ptr<LogsManagementUnit> createServerUnit(QnMediaServerResourcePtr server);

    void setChecked(bool isChecked);
    void setState(DownloadState state);
    void setSettings(const std::optional<nx::vms::api::ServerLogSettings>& settings);

private:
    mutable nx::Mutex m_mutex;
    QnMediaServerResourcePtr m_server;

    bool m_checked{false};
    DownloadState m_state{DownloadState::none};
    std::optional<nx::vms::api::ServerLogSettings> m_settings;
};
using LogsManagementUnitPtr = std::shared_ptr<LogsManagementUnit>;

/**
 * 
 */
class NX_VMS_CLIENT_DESKTOP_API LogsManagementWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class State
    {
        empty,
        hasSelection,
        loading,
        finished,
        hasErrors,
    };

public:
    LogsManagementWatcher(QObject* parent = nullptr);

    QList<LogsManagementUnitPtr> items() const;
    QList<LogsManagementUnitPtr> checkedItems() const;

    QString path() const;

    void setItemIsChecked(LogsManagementUnitPtr item, bool checked);

    void startDownload(const QString& path);
    void cancelDownload();
    void restartFailed();
    void completeDownload();

    void applySettings(const ConfigurableLogSettings& settings);

signals:
    void stateChanged(State state);
    void progressChanged(double progress);
    void itemListChanged();
    void itemChanged(int idx);

private:
    void loadInitialSettings();
    void loadServerSettings(const QnUuid& serverId);
    
    void onReceivedServerLogSettings(
        const QnUuid& serverId,
        const nx::vms::api::ServerLogSettings& settings);

    bool storeServerSettings(
        LogsManagementUnitPtr server,
        const ConfigurableLogSettings& settings);

    void downloadClientLogs(LogsManagementUnitPtr unit);
    void downloadServerLogs(LogsManagementUnitPtr unit);

    void updateDownloadState();

private:
    mutable nx::Mutex m_mutex;
    State m_state;

    LogsManagementUnitPtr m_client;
    QList<LogsManagementUnitPtr> m_servers;
    QString m_path;
};

} // namespace nx::vms::client::desktop
