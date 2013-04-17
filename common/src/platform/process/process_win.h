#ifndef QN_WINDOWS_PROCESS_H
#define QN_WINDOWS_PROCESS_H

#include "platform_process.h"

class QnWindowsProcessPrivate;

class QnWindowsProcess: public QnPlatformProcess {
    Q_OBJECT;
public:
    QnWindowsProcess(QProcess *process, QObject *parent = NULL);
    virtual ~QnWindowsProcess();

    virtual qint64 pid() const override;
    virtual Priority priority() const override;
    virtual void setPriority(Priority priority) override;

private slots:
    void at_process_stateChanged();

private:
    Q_DECLARE_PRIVATE(QnWindowsProcess);

    QScopedPointer<QnWindowsProcessPrivate> d_ptr;
};

#endif // QN_WINDOWS_PROCESS_H
