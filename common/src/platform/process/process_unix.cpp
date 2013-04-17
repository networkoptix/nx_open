#include "process_unix.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

namespace {
    enum {
        InvalidNiceValue = 0xDEADBEEF
    };
}

// -------------------------------------------------------------------------- //
// QnLinuxProcessPrivate
// -------------------------------------------------------------------------- //
class QnLinuxProcessPrivate {
public:
    QnLinuxProcessPrivate(): initialized(false), current(false), valid(false), pid(-1) {}

private:
    void tryInitialize() {
        if(initialized)
            return;

        if(!process) {
            if(current) {
                pid = getpid();
                initialized = true;
                valid = true;
            } else {
                initialized = true;
                valid = false;
            }

            return;
        }

        QProcess *process = this->process.data();
        if(process->state() == QProcess::NotRunning)
            return; /* Cannot initialize yet. */

        if(pid = process->pid()) {
            initialized = true;
            valid = true;
        } else {
            initialized = true;
            valid = false;
        }
    }

    static int qnToSystemPriority(QnPlatformProcess::Priority priority) {
        switch(priority) {
        case QnPlatformProcess::IdlePriority:
            return 19;
        case QnPlatformProcess::LowestPriority:
            return 13;
        case QnPlatformProcess::LowPriority:
            return 6;
        case QnPlatformProcess::NormalPriority:
            return 0;
        case QnPlatformProcess::HighPriority:
            return -6;
        case QnPlatformProcess::HighestPriority:
            return -13;
        case QnPlatformProcess::TimeCriticalPriority:
            return -20;
        default:
            return InvalidNiceValue;
        }
    }

    static QnPlatformProcess::Priority systemToQnPrioriy(int priority) {
        switch(priority) {
        case 19: case 18: case 17: case 16:
            return QnPlatformProcess::IdlePriority;
        case 15: case 14: case 13: case 12: case 11: case 10:
            return QnPlatformProcess::LowestPriority:
        case 9: case 8: case 7: case 6: case 5: case 4: case 3:
            return QnPlatformProcess::LowPriority;
        case 2: case 1: case 0: case -1: case -2: case -3:
            return QnPlatformProcess::NormalPriority;
        case -4: case -5: case -6: case -7: case -8: case -9:
            return QnPlatformProcess::HighPriority;
        case -10: case -11: case -12: case -13: case -14: case -15: case -16:
            return QnPlatformProcess::HighestPriority;
        case -17: case -18: case -19: case -20:
            return QnPlatformProcess::TimeCriticalPriority;
        default:
            return QnPlatformProcess::InvalidPriority;
        }
    }

private:
    friend class QnLinuxProcess;

    bool initialized;
    bool current;
    bool valid;
    qint64 pid;
    QWeakPointer<QProcess> process;
};


// -------------------------------------------------------------------------- //
// QnLinuxProcess
// -------------------------------------------------------------------------- //
QnLinuxProcess::QnLinuxProcess(QProcess *process, QObject *parent):
    base_type(parent),
    d_ptr(new QnLinuxProcessPrivate())
{
    Q_D(QnLinuxProcess);
    d->process = process;
    d->current = process == NULL;

    if(process)
        connect(process, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(at_process_stateChanged()));

    d->tryInitialize();
}

QnLinuxProcess::~QnLinuxProcess() {
    return;
}

qint64 QnLinuxProcess::pid() const override { 
    return d_func()->pid;
}

QnPlatformProcess::Priority QnLinuxProcess::priority() const override {
    Q_D(const QnLinuxProcess);
    if(!d->valid)
        return InvalidPriority;

    errno = 0;
    int systemPriority = getpriority(PRIO_PROCESS, d->pid);
    if(errno != 0) {
        return InvalidPriority;
    } else {
        return d->systemToQnPrioriy(systemPriority);
    }
}

void QnLinuxProcess::setPriority(Priority priority) override { 
    Q_D(QnLinuxProcess);
    if(!d->valid) {
        qnWarning("Process is not valid, could not set priority.");
        return;
    }

    int systemPriority = d->qnToSystemPriority(priority);
    if(systemPriority == InvalidNiceValue) {
        qnWarning("Invalid process priority value '%1'.", static_cast<int>(priority));
        return;
    }

    if(setpriority(PRIO_PROCESS, d->pid, systemPriority) != 0) {
        qnWarning("Could not set priority for process.");
        return;
    }
}

void QnLinuxProcess::at_process_stateChanged() {
    d_func()->tryInitialize();
}
