#include "windows_monitor.h"

#include <cassert>

#include <utils/common/warnings.h>

#include <Windows.h>

/* These defines are from win32_sigar.c. */

#define PERF_TITLE_DISK_KEY  L"236"

#define PdhFirstObject(block) \
    ((PERF_OBJECT_TYPE *)((BYTE *) block + block->HeaderLength))

#define PdhNextObject(object) \
    ((PERF_OBJECT_TYPE *)((BYTE *) object + object->TotalByteLength))

#define PdhFirstCounter(object) \
    ((PERF_COUNTER_DEFINITION *)((BYTE *) object + object->HeaderLength))

#define PdhNextCounter(counter) \
    ((PERF_COUNTER_DEFINITION *)((BYTE *) counter + counter->ByteLength))

#define PdhGetCounterBlock(inst) \
    ((PERF_COUNTER_BLOCK *)((BYTE *) inst + inst->ByteLength))

#define PdhFirstInstance(object) \
    ((PERF_INSTANCE_DEFINITION *)((BYTE *) object + object->DefinitionLength))

#define PdhNextInstance(inst) \
    ((PERF_INSTANCE_DEFINITION *)((BYTE *)inst + inst->ByteLength + \
    PdhGetCounterBlock(inst)->ByteLength))

#define PdhInstanceName(inst) \
    ((wchar_t *)((BYTE *)inst + inst->NameOffset))


#define INVOKE(expression)                                                      \
    (d_func()->checkError(#expression, expression))

class QnWindowsMonitorPrivate {
public:
    struct PerfOffsetMap {
        int objectSize, counterCount;

        QHash<int, int> counterOffsets;
    };

    QnWindowsMonitorPrivate(): handle(0) {
    }

    virtual ~QnWindowsMonitorPrivate() {
        if(handle != 0 && handle != INVALID_HANDLE_VALUE)
            INVOKE(RegCloseKey(handle));
    }

    void ensurePerfHandle() {
        if(handle == 0 || handle == INVALID_HANDLE_VALUE)
            return;

        if(INVOKE(RegConnectRegistryW(NULL, HKEY_PERFORMANCE_DATA, &handle)) != ERROR_SUCCESS)
            handle = reinterpret_cast<HKEY>(INVALID_HANDLE_VALUE);
    }

    DWORD checkError(const char *expression, DWORD status) const {
        assert(expression);

        if(status == ERROR_SUCCESS)
            return status;

        QByteArray function(expression);
        int index = function.indexOf('(');
        if(index != -1)
            function.truncate(index);

        LPWSTR buffer;
        if(FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, status, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL) == 0)
            buffer = NULL; /* Error in FormatMessage. */

        if(buffer) {
            qnWarning("Error in %1: %2 (%3).", function, status, buffer);
            LocalFree(buffer);
        } else {
            qnWarning("Error in %1: %2.", function, status);
        }

        return status;
    }

    bool queryPerfValue(LPCWSTR name, LPDWORD type, QByteArray *value, bool resize = true) {
        assert(name && type && value);

        ensurePerfHandle();
        if(handle == INVALID_HANDLE_VALUE)
            return false;

        value->resize(value->capacity());
        DWORD size = value->size();
        LPBYTE data = reinterpret_cast<LPBYTE>(value->data());

        LSTATUS status = RegQueryValueExW(handle, name, NULL, type, data, &size);
        if(resize && status == ERROR_MORE_DATA) {
            value->resize(size + 1);
            return queryPerfValue(name, type, value, false);
        }

        if(status != ERROR_SUCCESS) {
            checkError("RegQueryValueExW", status);
            return false;
        }

        value->resize(size);
        return;
    }

    PERF_OBJECT_TYPE *queryPerfObject(LPCWSTR name, QByteArray *buffer) {
        assert(name && buffer);

        DWORD type;
        if(!queryPerfValue(name, &type, buffer))
            return NULL;
    
        PERF_DATA_BLOCK *block = reinterpret_cast<PERF_DATA_BLOCK *>(buffer->data());
        if (block->NumObjectTypes == 0) {
            qnWarning("No counters defined for title '%1'.", name);
            return NULL;
        }
    
        PERF_OBJECT_TYPE *object = PdhFirstObject(block);

        /* Code from win32_sigar.c.
         * 
         * Only seen on windows 2003 server when pdh.dll
         * functions are in use by the same process. */
        if(object->NumInstances == PERF_NO_INSTANCES) {
            for (DWORD i = 0; i < block->NumObjectTypes; i++) {
                if (object->NumInstances != PERF_NO_INSTANCES)
                    return object;
                object = PdhNextObject(object);
            }
            return NULL;
        } else {
            return object;
        }
    }



private:
    const QnWindowsMonitorPrivate *d_func() const { return this; } /* For INVOKE to work. */
    
private:
    HKEY handle;
    QByteArray perfBuffer;

private:
    Q_DECLARE_PUBLIC(QnWindowsMonitor);
    QnWindowsMonitor *q_ptr;
};


QnWindowsMonitor::QnWindowsMonitor(QObject *parent):
    base_type(parent),
    d_ptr(new QnWindowsMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

QnWindowsMonitor::~QnWindowsMonitor() {
    return;
}

QList<QnPlatformMonitor::Hdd> QnWindowsMonitor::hdds() {
    Q_D(QnWindowsMonitor);

    d->ensurePerfHandle();

    QList<QnPlatformMonitor::Hdd> result;

    PERF_OBJECT_TYPE *object = d->queryPerfObject(PERF_TITLE_DISK_KEY, &d->perfBuffer);

}

qreal QnWindowsMonitor::totalHddLoad(const Hdd &hdd) {

}

