#ifndef QN_UNIX_PROCESS_H
#define QN_UNIX_PROCESS_H

#include "platform_process.h"

class QnUnixProcessPrivate;

class QnUnixProcess: public QnPlatformProcess {
    Q_OBJECT;
    typedef QnPlatformProcess base_type;

public:
    QnUnixProcess(QProcess *process, QObject *parent = NULL);
    virtual ~QnUnixProcess();

    virtual qint64 pid() const override;
    virtual Priority priority() const override;
    virtual void setPriority(Priority priority) override;

private slots:
    void at_process_stateChanged();

protected:
    Q_DECLARE_PRIVATE(QnUnixProcess);
    QScopedPointer<QnUnixProcessPrivate> d_ptr;
};

#endif // QN_UNIX_PROCESS_H
