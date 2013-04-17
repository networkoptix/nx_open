#ifndef QN_LINUX_PROCESS_H
#define QN_LINUX_PROCESS_H

#include "platform_process.h"

class QnLinuxProcessPrivate;

class QnLinuxProcess: public QnPlatformProcess {
    Q_OBJECT;
    typedef QnPlatformProcess base_type;

public:
    QnLinuxProcess(QProcess *process, QObject *parent = NULL);
    virtual ~QnLinuxProcess();

    virtual qint64 pid() const override;
    virtual Priority priority() const override;
    virtual void setPriority(Priority priority) override;

private slots:
    void at_process_stateChanged();

protected:
    Q_DECLARE_PRIVATE(QnLinuxProcess);
    QScopedPointer<QnLinuxProcessPrivate> d_ptr;
};

#endif // QN_LINUX_PROCESS_H
