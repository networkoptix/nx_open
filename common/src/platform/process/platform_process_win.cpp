#include "platform_process_win.h"

#include <Windows.h>

#include <utils/common/warnings.h>

#include <QtCore/QProcess>
#include <QtCore/QPointer>

#define INVALID_PRIORITY_CLASS 0xDEADF00D


// -------------------------------------------------------------------------- //
// QnWindowsProcessPrivate
// -------------------------------------------------------------------------- //
class QnWindowsProcessPrivate {
public:
    QnWindowsProcessPrivate(): initialized(false), current(false), valid(false), pid(-1), handle(INVALID_HANDLE_VALUE) {}

private:
    void tryInitialize() {
        if(initialized)
            return;

        if(!process) {
            if(current) {
                handle = GetCurrentProcess();
                pid = GetCurrentProcessId();
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

        if(PROCESS_INFORMATION *info = process->pid()) {
            pid = info->dwProcessId;
            handle = info->hProcess;
            initialized = true;
            valid = true;
        } else {
            initialized = true;
            valid = false;
        }
    }

    static DWORD qnToSystemPriority(QnPlatformProcess::Priority priority) {
        switch(priority) {
        case QnPlatformProcess::IdlePriority:
            return IDLE_PRIORITY_CLASS;
        case QnPlatformProcess::LowestPriority:
        case QnPlatformProcess::LowPriority:
            return BELOW_NORMAL_PRIORITY_CLASS;
        case QnPlatformProcess::NormalPriority:
            return NORMAL_PRIORITY_CLASS;
        case QnPlatformProcess::HighPriority:
            return ABOVE_NORMAL_PRIORITY_CLASS;
        case QnPlatformProcess::HighestPriority:
            return HIGH_PRIORITY_CLASS;
        case QnPlatformProcess::TimeCriticalPriority:
            return REALTIME_PRIORITY_CLASS;
        default:
            return INVALID_PRIORITY_CLASS;
        }
    }

    static QnPlatformProcess::Priority systemToQnPrioriy(DWORD priority) {
        switch(priority) {
        case IDLE_PRIORITY_CLASS:
            return QnPlatformProcess::IdlePriority;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return QnPlatformProcess::LowPriority;
        case NORMAL_PRIORITY_CLASS:
            return QnPlatformProcess::NormalPriority;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return QnPlatformProcess::HighPriority;
        case HIGH_PRIORITY_CLASS:
            return QnPlatformProcess::HighestPriority;
        case REALTIME_PRIORITY_CLASS:
            return QnPlatformProcess::TimeCriticalPriority;
        default:
            return QnPlatformProcess::InvalidPriority;
        }
    }

private:
    friend class QnWindowsProcess;

    bool initialized;
    bool current;
    bool valid;
    qint64 pid;
    HANDLE handle;
    QPointer<QProcess> process;
};


// -------------------------------------------------------------------------- //
// QnWindowsProcess
// -------------------------------------------------------------------------- //
QnWindowsProcess::QnWindowsProcess(QProcess *process, QObject *parent):
    QnPlatformProcess(parent),
    d_ptr(new QnWindowsProcessPrivate())
{
    Q_D(QnWindowsProcess);
    d->process = process;
    d->current = process == NULL;

    if(process)
        connect(process, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(at_process_stateChanged()));

    d->tryInitialize();
}

QnWindowsProcess::~QnWindowsProcess() {
    return;
}

qint64 QnWindowsProcess::pid() const {
    return d_func()->pid;
}

QnPlatformProcess::Priority QnWindowsProcess::priority() const {
    Q_D(const QnWindowsProcess);
    if(!d->valid)
        return InvalidPriority;

    DWORD systemPriority = GetPriorityClass(d->handle);
    return d->systemToQnPrioriy(systemPriority);
}

void QnWindowsProcess::setPriority(Priority priority) {
    Q_D(QnWindowsProcess);
    if(!d->valid) {
        qnWarning("Process is not valid, could not set priority.");
        return;
    }

    DWORD systemPriority = d->qnToSystemPriority(priority);
    if(systemPriority == INVALID_PRIORITY_CLASS) {
        qnWarning("Invalid process priority value '%1'.", static_cast<int>(priority));
        return;
    }

    if(SetPriorityClass(d->handle, systemPriority) == 0) {
        qnWarning("Could not set priority for process.");
        return;
    }
}

void QnWindowsProcess::at_process_stateChanged() {
    d_func()->tryInitialize();
}

