#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtCore/QTimer>

#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>

#include "../redux/time_synchronization_widget_state.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

class TimeSynchronizationServersModel:
    public ScopedModelOperations<QAbstractTableModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractTableModel>;
    using State = TimeSynchronizationWidgetState;

public:
    enum Columns
    {
        CheckboxColumn,
        NameColumn,
        TimezoneColumn,
        DateColumn,
        OsTimeColumn,
        VmsTimeColumn,
        ColumnCount
    };

    enum DataRole
    {
        IpAddressRole = Qt::UserRole,
        ServerIdRole,
        ServerOnlineRole,
        TimeOffsetRole
    };

    explicit TimeSynchronizationServersModel(QObject* parent = nullptr);

    virtual int columnCount(const QModelIndex& parent) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    void loadState(const State& state);

signals:
    void serverSelected(const QnUuid& id);

private:
    bool isValid(const QModelIndex& index) const;

private:
    milliseconds m_vmsTime = milliseconds(0);
    QList<State::ServerInfo> m_servers;
    QnUuid m_selectedServer;
};

} // namespace nx::vms::client::desktop
