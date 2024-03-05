// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QString>

#include <client/client_globals.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/log_settings.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>

namespace nx::vms::client::desktop {

class LogsManagementModel: public QAbstractTableModel
{
    Q_OBJECT
    typedef QAbstractTableModel base_type;

public:
    enum Columns
    {
        CheckBoxColumn,
        NameColumn,
        LogLevelColumn,
        RetryColumn,
        StatusColumn,
        ColumnCount
    };

    enum Roles
    {
        IpAddressRole = Qt::UserRole + 1,
        EnabledRole,
        RetryRole,
        DownloadingRole,
        QnUuidRole,
    };

    explicit LogsManagementModel(QObject* parent, LogsManagementWatcher* watcher);

    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void setFilterText(const QString& text);

    static QList<nx::log::Level> logLevels();
    static QString logLevelName(nx::log::Level level);

private:
    void onItemsListChanged();
    void onItemsChanged(QList<LogsManagementUnitPtr> items);
    QString logLevelTooltip(nx::log::Level level) const;

private:
    QPointer<LogsManagementWatcher> m_watcher;
    QList<LogsManagementUnitPtr> m_items;

    QString m_filterText = {};
};

} // namespace nx::vms::client::desktop
