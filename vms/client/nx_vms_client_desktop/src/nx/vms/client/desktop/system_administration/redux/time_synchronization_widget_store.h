#pragma once

#include <QtCore/QObject>

#include "time_synchronization_widget_state.h"

namespace nx::vms::client::desktop {

class TimeSynchronizationWidgetStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using State = TimeSynchronizationWidgetState;

    struct TimeOffsetInfo
    {
        QnUuid serverId;
        qint64 osTimeOffset = 0;
        qint64 vmsTimeOffset = 0;
    };
    using TimeOffsetInfoList = QList<TimeOffsetInfo>;

    explicit TimeSynchronizationWidgetStore(QObject* parent = nullptr);
    virtual ~TimeSynchronizationWidgetStore() override;

    const TimeSynchronizationWidgetState& state() const;

    void initialize(
        bool isTimeSynchronizationEnabled,
        const QnUuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    void addServer(const State::ServerInfo& serverInfo);
    void removeServer(const QnUuid& id);
    void setServerOnline(const QnUuid &id, bool isOnline);

    void applyChanges();
    void setReadOnly(bool value);
    void setSyncTimeWithInternet(bool value);
    void disableSync();
    void selectServer(const QnUuid& serverId);

    void setVmsTime(std::chrono::milliseconds value);
    void setTimeOffsets(const TimeOffsetInfoList &offsetList);

signals:
    void stateChanged(const TimeSynchronizationWidgetState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
