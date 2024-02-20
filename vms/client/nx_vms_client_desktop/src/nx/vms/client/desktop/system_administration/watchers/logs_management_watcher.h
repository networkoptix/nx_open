// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QThread>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

struct ConfigurableLogSettings;

class ClientLogCollector: public QThread
{
    Q_OBJECT
    using base_type = QThread;

public:
    ClientLogCollector(const QString& target, QObject *parent = nullptr);
    virtual ~ClientLogCollector() override;

    void pleaseStop();

protected:
    virtual void run() override;

signals:
    void progressChanged(double progress);
    void success();
    void error();

private:
    const QString m_target;
    std::atomic<bool> m_cancelled{false};
};

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
        hasLocalErrors,
        hasErrors
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

        enum class ErrorType
        {
            local,
            remote,
        };

        nx::Uuid id() const;
        bool isChecked() const;
        QnMediaServerResourcePtr server() const;
        std::optional<nx::vms::api::ServerLogSettings> settings() const;
        DownloadState state() const;
        bool errorIsLocal() const;

        struct Private;
        Private* data();

    private:
        nx::utils::ImplPtr<Private> d;
    };
    using UnitPtr = std::shared_ptr<Unit>;

public:
    LogsManagementWatcher(SystemContext* context, QObject* parent = nullptr);
    ~LogsManagementWatcher();

    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

    QList<UnitPtr> items() const;
    QList<UnitPtr> checkedItems() const;
    QList<UnitPtr> itemsWithErrors() const;

    QString path() const;

    void setItemIsChecked(UnitPtr item, bool checked);

    void setAllItemsAreChecked(bool checked);
    Qt::CheckState itemsCheckState() const;

    void startDownload(const QString& path);
    void cancelDownload();
    void restartFailed();
    void restartById(const nx::Uuid& id);
    void completeDownload();

    void setUpdatesEnabled(bool enabled);

    // parentWidget may be used to show a password request dialog, and therefore is mandatory.
    void applySettings(
        const ConfigurableLogSettings& settings,
        QWidget* parentWidget);

    static nx::utils::log::Level defaultLogLevel();
    static nx::vms::api::ServerLogSettings clientLogSettings();

    UnitPtr clientUnit() const;
    void storeClientSettings(const ConfigurableLogSettings& settings);
    nx::vms::client::desktop::LogsManagementWatcher::State state() const;

signals:
    void stateChanged(nx::vms::client::desktop::LogsManagementWatcher::State state);
    void progressChanged(double progress);

    void itemListChanged();
    void itemsChanged(QList<UnitPtr> units);

    void selectionChanged(Qt::CheckState state);

private:
    void onReceivedServerLogSettings(
        const nx::Uuid& serverId,
        const nx::vms::api::ServerLogSettings& settings);

    void onSentServerLogSettings(
        const nx::Uuid& serverId,
        bool success);

    void downloadClientLogs(const QString& folder, UnitPtr unit);
    void downloadServerLogs(const QString& folder, UnitPtr unit);

    void updateDownloadState();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
using LogsManagementUnitPtr = LogsManagementWatcher::UnitPtr;

} // namespace nx::vms::client::desktop
