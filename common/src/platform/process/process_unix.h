#ifndef QN_LINUX_PROCESS_H
#define QN_LINUX_PROCESS_H

#include <QtCore/QtGlobal>

#ifndef Q_OS_LINUX
#include "platform_process.h"

class QnLinuxProcess: public QnPlatformProcess {
    Q_OBJECT;
public:
    QnLinuxProcess(QProcess *process, QObject *parent = NULL): QnPlatformProcess(parent) {}

    virtual qint64 pid() const override { return -1; }
    virtual Priority priority() const override { return InvalidPriority; }
    virtual void setPriority(Priority priority) override { return; }
};

#endif // Q_OS_LINUX

#endif // QN_LINUX_PROCESS_H
