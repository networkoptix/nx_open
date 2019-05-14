#ifndef __SYSTEM_EVENT_LOG_READER_H__
#define __SYSTEM_EVENT_LOG_READER_H__

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

#include <QtCore/QString>

class SystemEventLogReader: public QObject
{
    Q_OBJECT
public:
    SystemEventLogReader(QString logName, nx::vms::event::EventReason reason);
    virtual ~SystemEventLogReader();

    bool subscribeRaidEvents();
    void unsubscribeRaidEvents();
    virtual bool messageIsSignificant(const QString& xmlMessage) { return true; }
    QString makeDebugMessage(const QString& text);

    QString logName() const { return m_logName; }
    nx::vms::event::EventReason reason() const { return m_reason; }

signals:
    // Signal that is emitted if event has been added to log and the event is significant.
    void eventOccurred(const QnResourcePtr &storageRes, nx::vms::event::EventReason reason);

private:
    void* m_hSubscription = nullptr; //< Used in Windows only and has a type EVT_HANDLE.
    QString m_logName; //< Log name from which the events are occurred.
    nx::vms::event::EventReason m_reason; //< Reason used in signal.
};

class RaidEventLogReader: public SystemEventLogReader
{
public:
    RaidEventLogReader(QString logName) :
        SystemEventLogReader(std::move(logName), nx::vms::event::EventReason::raidStorageError)
    {
    }
    virtual bool messageIsSignificant(const QString& xmlMessage) override
    {
        // TODO #szaitsev: after receiving some examples of incoming XMLs, message analyzer
        // should be written.
        return true;
    }
};

#endif // __STORAGE_MANAGER_H__
