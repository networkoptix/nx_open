// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QAbstractTableModel>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/log_settings.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class LogsManagementUnit
{
public:
    enum class DownloadState
    {
        none,
        pending,
        complete,
        error,
    };

    static std::shared_ptr<LogsManagementUnit> createClientUnit();
    static std::shared_ptr<LogsManagementUnit> createServerUnit(QnMediaServerResourcePtr server);

    QnMediaServerResourcePtr server() const;

    bool isChecked() const;
    void setChecked(bool isChecked);

    DownloadState state() const;
    void setState(DownloadState state);

    std::optional<nx::vms::api::ServerLogSettings> settings() const;
    void setSettings(const std::optional<nx::vms::api::ServerLogSettings>& settings);

private:
    mutable nx::Mutex m_mutex;
    QnMediaServerResourcePtr m_server;

    bool m_checked{false};
    DownloadState m_state{DownloadState::none};
    std::optional<nx::vms::api::ServerLogSettings> m_settings;
};
using LogsManagementUnitPtr = std::shared_ptr<LogsManagementUnit>;

class LogsManagementModel: public QAbstractTableModel, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QAbstractTableModel base_type;

public:
    enum Columns
    {
        NameColumn,
        LogLevelColumn,
        CheckBoxColumn,
        StatusColumn,
        ColumnCount
    };

    explicit LogsManagementModel(QObject* parent);

    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    static QList<nx::utils::log::Level> logLevels();
    static QString logLevelName(nx::utils::log::Level level);

private:
    LogsManagementUnitPtr itemForRow(int row) const;

private:
    LogsManagementUnitPtr m_client;
    std::vector<LogsManagementUnitPtr> m_servers;
};

} // namespace nx::vms::client::desktop
