#include "windows_monitor.h"

#include <cassert>

#include <utils/common/warnings.h>

#include <Windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#pragma comment(lib, "pdh.lib")

#define INVOKE(expression)                                                      \
    (d_func()->checkError(#expression, expression))

class QnWindowsMonitorPrivate {
public:
    enum {
        UpdateIntervalMSec = 25
    };

    QnWindowsMonitorPrivate(): 
        pdhLibrary(0), 
        query(0),
        diskCounter(0),
        lastCollectTimeMSec(0)
    {
        pdhLibrary = LoadLibraryW(L"pdh.dll");
        if(!pdhLibrary)
            checkError("LoadLibrary", GetLastError());
    }

    virtual ~QnWindowsMonitorPrivate() {
        if(query != 0 && query != INVALID_HANDLE_VALUE)
            INVOKE(PdhCloseQuery(query));

        if(pdhLibrary)
            FreeLibrary(pdhLibrary);
    }

    void ensureQuery() {
        if(query != 0)
            return;

        if(INVOKE(PdhOpenQueryW(NULL, 0, &query)) != ERROR_SUCCESS) {
            query = INVALID_HANDLE_VALUE;
            diskCounter = INVALID_HANDLE_VALUE;
            return;
        }

        /* Note:
         * 
         * 234 = PhysicalDisk
         * 200 = % Disk Time
         * 
         * See http://support.microsoft.com/?scid=kb%3Ben-us%3B287159&x=11&y=9  */
        QString diskQuery = QString(QLatin1String("\\%1(*)\\%2")).arg(perfName(234)).arg(perfName(200));
        if(INVOKE(PdhAddCounterW(query, reinterpret_cast<LPCWSTR>(diskQuery.utf16()), 0, &diskCounter)) != ERROR_SUCCESS)
            diskCounter = INVALID_HANDLE_VALUE;
    }

    void collectQuery() {
        ensureQuery();
        if(query == INVALID_HANDLE_VALUE)
            return;

        ULONGLONG timeMSec = GetTickCount64();
        if(timeMSec - lastCollectTimeMSec < UpdateIntervalMSec)
            return; /* Don't update too often. */

        lastCollectTimeMSec = timeMSec;
        if(INVOKE(PdhCollectQueryData(query)) != ERROR_SUCCESS) {
            diskData.clear();
            return;
        }

        updateCounterValues(diskCounter, &diskData);
    }

    DWORD checkError(const char *expression, DWORD status) const {
        assert(expression);

        if(status == ERROR_SUCCESS)
            return status;

        QByteArray function(expression);
        int index = function.indexOf('(');
        if(index != -1)
            function.truncate(index);

        LPWSTR buffer = NULL;
        if(FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, pdhLibrary, status, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL) == 0)
            buffer = NULL; /* Error in FormatMessage. */

        if(buffer) {
            qnWarning("Error in %1: %2 (%3).", function, status, buffer);
            LocalFree(buffer);
        } else {
            qnWarning("Error in %1: %2.", function, status);
        }
    }

    QString perfName(DWORD index) {
        WCHAR buffer[PDH_MAX_COUNTER_NAME];
        DWORD size = sizeof(buffer);

        if(INVOKE(PdhLookupPerfNameByIndexW(NULL, index, buffer, &size)) !=  ERROR_SUCCESS)
            return QString();

        return QString::fromWCharArray(buffer);
    }

    int updateCounterValues(PDH_HCOUNTER counter, QByteArray *data) {
        assert(data);

        DWORD bufferSize = 0, itemCount = 0;

        if(INVOKE(PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, NULL) != ERROR_SUCCESS)) {
            data->clear();
            return 0;
        }

        /* Note that additional symbol is important here.
         * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa372642%28v=vs.85%29.aspx. */
        data->resize(bufferSize + sizeof(WCHAR)); 
        bufferSize = data->size();
        itemCount = 0;

        if(INVOKE(PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, reinterpret_cast<PDH_RAW_COUNTER_ITEM_W *>(data->data())) != ERROR_SUCCESS)) {
            data->clear();
            return 0;
        }

        return itemCount;
    }

private:
    const QnWindowsMonitorPrivate *d_func() const { return this; } /* For INVOKE to work. */
    
private:
    HMODULE pdhLibrary;
    PDH_HQUERY query;
    PDH_HCOUNTER diskCounter;
    
    ULONGLONG lastCollectTimeMSec;
    QByteArray diskData;

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

    QList<QnPlatformMonitor::Hdd> result;

    d->collectQuery();
    if(d->diskCounter == INVALID_HANDLE_VALUE)
        return result;

    

//    PERF_OBJECT_TYPE *object = d->queryPerfObject(PERF_TITLE_DISK_KEY, &d->perfBuffer);

}

qreal QnWindowsMonitor::totalHddLoad(const Hdd &hdd) {
    return 0.0;
}

