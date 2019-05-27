#ifndef __SYSTEM_EVENT_LOG_READER_H__
#define __SYSTEM_EVENT_LOG_READER_H__

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

#include <QtCore/QString>

#ifdef WIN32
    #include <winevt.h>
#endif

struct notificationInfo
{
    QString providerName;
    QString data;
    int eventId = 0;
    int level = 0;
};

class SystemEventLogReader: public QObject
{
    Q_OBJECT
public:
    SystemEventLogReader(QString logName, QString providerName, int maxLevel,
        nx::vms::event::EventReason reason);
    virtual ~SystemEventLogReader();

    bool subscribe();
    void unsubscribe();
    virtual bool messageIsSignificant(
        const QString& xmlMessage, const notificationInfo& parsedMessage);

    QString logName() const { return m_logName; }
    QString providerName() const { return m_providerName; }
    int maxLevel() const { return m_maxLevel; }
    nx::vms::event::EventReason reason() const { return m_reason; }

signals:
    // Signal that is emitted if event has been added to log and the event is significant.
    void eventOccurred(const QString &description, nx::vms::event::EventReason reason);

private:
#ifdef WIN32
    static DWORD WINAPI SubscriptionCallback(
        EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE hEvent);
#endif
    notificationInfo parseXmlMessage(const QString& xmlMessage);
    QString makeDebugMessage(const QString& text);

private:
    void* m_hSubscription = nullptr; //< Used in Windows only and has a type EVT_HANDLE.

    /** Name of the log which the events should be read from. */
    QString m_logName;

    /** Name of the application that generates the events to read. */
    QString m_providerName;

    /**
     * Maximum level. All events with the level higher then maxLevel are ignored by this reader.
     * The levels are:
     * 0 - not used
     * 1 - Critical (aka Fatal)
     * 2 - Error (aka Critical)
     * 3 - Warning
     * 4 - Information
     * 5 - Verbose (aka Progress)
     */
    int m_maxLevel;
    nx::vms::event::EventReason m_reason; //< Reason used in signal.
};

class RaidEventLogReader: public SystemEventLogReader
{
public:
    RaidEventLogReader(QString logName, QString providerName, int maxLevel);

    virtual bool messageIsSignificant(
        const QString& xmlMessage, const notificationInfo& parsedMessage) override;
};

#endif // __STORAGE_MANAGER_H__
