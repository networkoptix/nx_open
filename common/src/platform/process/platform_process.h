#ifndef QN_PLATFORM_PROCESS_H
#define QN_PLATFORM_PROCESS_H

#include <QtCore/QObject>
#include <QtCore/QThread>

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

    virtual qint64 pid() const = 0;
    virtual Priority priority() const = 0;
    virtual void setPriority(Priority priority) = 0;
};


#endif // QN_PLATFORM_PROCESS_H

