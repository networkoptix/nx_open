#pragma once

#include <nx/vms/api/rules/event_info.h>

#include <chrono>

namespace nx::vms::rules {

class NX_VMS_RULES_API BasicEvent: public QObject
{
    Q_OBJECT

public:
    BasicEvent(nx::vms::api::rules::EventInfo &info);

    QString type() const;

protected:
    BasicEvent(const QString &type);

private:
    QString m_type;

    std::chrono::milliseconds m_timestamp;
    QStringList m_source;
    bool m_state;
};

class NX_VMS_RULES_API DebugEvent: public BasicEvent
{
    Q_OBJECT

    Q_PROPERTY(QString action READ action)
    Q_PROPERTY(qint64 value READ value)

public:
    DebugEvent(const QString &action, qint64 value):
        BasicEvent("DebugEvent"),
        m_action(action),
        m_value(value)
    {
    }

    QString action() const
    {
        return m_action;
    }

    qint64 value() const
    {
        return m_value;
    }

private:
    QString m_action;
    qint64 m_value;
};

} // namespace nx::vms::rules