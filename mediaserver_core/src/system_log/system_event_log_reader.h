#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

#include <QtCore/QString>

#ifdef WIN32
    #include <winevt.h>
#endif

struct SystemEventNotificationInfo
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
    virtual bool isMessageSignificant(
        const QString& xmlMessage, const SystemEventNotificationInfo& parsedMessage);

    /** Name of the log which the events should be read from. */
    QString logName() const { return m_logName; }

    /** Name of the application that generates the events to read. */
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
    SystemEventNotificationInfo parseXmlMessage(const QString& xmlMessage);
    QString makeDebugMessage(const QString& text);

private:
    void* m_hSubscription = nullptr; //< Used in Windows only and has a type EVT_HANDLE.
    QString m_logName;
    QString m_providerName;

    /**
     * Maximum level. All events with the level higher then maxLevel are ignored by this reader.
     * The levels are:
     * 0 - not used
     * 1 - Critical (Fatal in Hanwha notation)
     * 2 - Error (Critical in Hanwha notation)
     * 3 - Warning
     * 4 - Information
     * 5 - Verbose (Progress in Hanwha notation)
     */
    int m_maxLevel = 5;

    // Reason used in signal.
    nx::vms::event::EventReason m_reason = nx::vms::event::EventReason::none;
};

class RaidEventLogReader: public SystemEventLogReader
{
public:
    RaidEventLogReader(QString logName, QString providerName);

    virtual bool isMessageSignificant(
        const QString& xmlMessage, const SystemEventNotificationInfo& parsedMessage) override;
};
