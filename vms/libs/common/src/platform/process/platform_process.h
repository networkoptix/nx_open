#ifndef QN_PLATFORM_PROCESS_H
#define QN_PLATFORM_PROCESS_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QMetaType>

class QProcess;

class QnPlatformProcess: public QObject {
    Q_OBJECT;
public:
    enum Priority {
        IdlePriority = QThread::IdlePriority,
        LowestPriority = QThread::LowestPriority,
        LowPriority = QThread::LowPriority,
        NormalPriority = QThread::NormalPriority,
        HighPriority = QThread::HighPriority,
        HighestPriority = QThread::HighestPriority,
        TimeCriticalPriority = QThread::TimeCriticalPriority,
        InvalidPriority = -1
    };

    QnPlatformProcess(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformProcess() {}

    /**
     * \returns                         PID of this process, or -1 if this 
     *                                  process is not valid (e.g. not yet started).
     */
    virtual qint64 pid() const = 0;

    /**
     * \returns                         Priority of this process, or <tt>InvalidPriority</tt> in case
     *                                  of an error.
     */
    virtual Priority priority() const = 0;

    /**
     * \param priority                  New priority for this process.
     */
    virtual void setPriority(Priority priority) = 0;
};

Q_DECLARE_METATYPE(QnPlatformProcess *)

#endif // QN_PLATFORM_PROCESS_H

