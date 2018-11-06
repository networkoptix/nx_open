#pragma once

#include <QtCore/QObject>

#include "time_syncronization_widget_state.h"

namespace nx::client::desktop {

class TimeSynchronizationWidgetStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    using State = TimeSynchronizationWidgetState;

    explicit TimeSynchronizationWidgetStore(QObject* parent = nullptr);
    virtual ~TimeSynchronizationWidgetStore() override;

    const TimeSynchronizationWidgetState& state() const;

    void initialize(
        bool isTimeSynchronizationEnabled,
        const QnUuid& primaryTimeServer,
        const QList<State::ServerInfo>& servers);

    void setReadOnly(bool value);
    void setSyncTimeWithInternet(bool value);
    void disableSync();
    void selectServer(const QnUuid& serverId);

    void setVmsTime(std::chrono::milliseconds value);

signals:
    void stateChanged(const TimeSynchronizationWidgetState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::client::desktop
