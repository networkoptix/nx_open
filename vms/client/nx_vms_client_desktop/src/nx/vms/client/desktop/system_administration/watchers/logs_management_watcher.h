// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

struct ConfigurableLogSettings;

class NX_VMS_CLIENT_DESKTOP_API LogsManagementWatcher:
    public QObject,
    public SystemContextAware
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

    class Unit
    {
    public:
        Unit();
        enum class DownloadState
        {
            none,
            pending,
            loading,
            complete,
            error,
        };

        QnUuid id() const;
        bool isChecked() const;
        QnMediaServerResourcePtr server() const;
        std::optional<nx::vms::api::ServerLogSettings> settings() const;
        DownloadState state() const;

        struct Private;
        Private* data();

    private:
        nx::utils::ImplPtr<Private> d;
    };
    using UnitPtr = std::shared_ptr<Unit>;

public:
    LogsManagementWatcher(SystemContext* context, QObject* parent = nullptr);
    ~LogsManagementWatcher();

    QList<UnitPtr> items() const;
    QList<UnitPtr> checkedItems() const;

    QString path() const;

    void setItemIsChecked(UnitPtr item, bool checked);

    void startDownload(const QString& path);
    void cancelDownload();
    void restartFailed();
    void completeDownload();

    void setUpdatesEnabled(bool enabled);

    void applySettings(const ConfigurableLogSettings& settings);

signals:
    void stateChanged(State state);
    void progressChanged(double progress);
    void itemListChanged();
    void itemChanged(int idx);

    void logLevelChanged();

private:
    void onReceivedServerLogSettings(
        const QnUuid& serverId,
        const nx::vms::api::ServerLogSettings& settings);

    void downloadClientLogs(UnitPtr unit);
    void downloadServerLogs(UnitPtr unit);

    void updateDownloadState();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
using LogsManagementUnitPtr = LogsManagementWatcher::UnitPtr;

} // namespace nx::vms::client::desktop
