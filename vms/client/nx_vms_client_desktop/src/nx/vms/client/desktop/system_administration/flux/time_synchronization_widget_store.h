// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include "time_synchronization_widget_state.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

class TimeSynchronizationWidgetStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using State = TimeSynchronizationWidgetState;

    struct TimeOffsetInfo
    {
        nx::Uuid serverId;
        milliseconds osTimeOffset = 0ms;
        milliseconds vmsTimeOffset = 0ms;
        milliseconds timeZoneOffset = 0ms;
    };
    using TimeOffsetInfoList = QList<TimeOffsetInfo>;

    explicit TimeSynchronizationWidgetStore(QObject* parent = nullptr);
    virtual ~TimeSynchronizationWidgetStore() override;

    const TimeSynchronizationWidgetState& state() const;

    void initialize(
        bool isTimeSynchronizationEnabled,
        const nx::Uuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    void addServer(const State::ServerInfo& serverInfo);
    void removeServer(const nx::Uuid& id);
    void setServerOnline(const nx::Uuid &id, bool isOnline);
    void setServerHasInternet(const nx::Uuid &id, bool hasInternet);

    void applyChanges();
    void setReadOnly(bool value);
    void setSyncTimeWithInternet(bool value);
    void disableSync();
    void selectServer(const nx::Uuid& serverId);

    void setBaseTime(milliseconds time);
    void setElapsedTime(milliseconds time);
    void setTimeOffsets(const TimeOffsetInfoList &offsetList, milliseconds baseTime);

signals:
    void stateChanged(const TimeSynchronizationWidgetState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
