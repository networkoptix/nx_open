#include "windows_monitor.h"

#ifdef Q_OS_WIN

#include <cassert>

#include <QtCore/QVector>
#include <QtCore/QHash>

#include <utils/common/warnings.h>

#include <Windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#ifdef _MSC_VER
#   pragma comment(lib, "pdh.lib")
#endif

namespace {
    bool parseDiskDescription(LPCWSTR description, int *id, LPCWSTR *partitions) {
        if(!description)
            return false;

        LPCWSTR pos = wcschr(description, L' ');
        if(!pos)
            return false;
        while(*pos == L' ')
            pos++;
        if(*pos == L'\0')
            return false;

        int localId = _wtoi(description);
        if(localId == 0 && description[0] != L'0')
            return false;
        
        if(partitions)
            *partitions = pos;
        if(id)
            *id = localId;
        return true;
    }

} // anonymous namespace

#define INVOKE(expression)                                                      \
    (d_func()->checkError(#expression, expression))


// -------------------------------------------------------------------------- //
// QnWindowsMonitorPrivate
// -------------------------------------------------------------------------- //
class QnWindowsMonitorPrivate {
public:
    enum {
        /** Minimal interval between consequent re-reads of the performance counter,
         * in milliseconds. */
        UpdateIntervalMSec = 25
    };

    typedef QnPlatformMonitor::Hdd Hdd;

    struct HddItem {
        HddItem() {}
        HddItem(const Hdd &hdd, const PDH_RAW_COUNTER &counter): hdd(hdd), counter(counter) {}

        Hdd hdd;
        PDH_RAW_COUNTER counter;
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
            itemByDiskId.clear();
            return;
        }

        lastItemByDiskId = itemByDiskId;
        readDiskCounterValues(diskCounter, &itemByDiskId);
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
        if(FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, pdhLibrary, status, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL) == 0) {
            //DWORD error = GetLastError();
            buffer = NULL; /* Error in FormatMessage. */
        }

        if(buffer) {
            QString message = QString::fromWCharArray(buffer);
            while(message.endsWith(QLatin1Char('\n')) || message.endsWith(QLatin1Char('\r')))
                message.truncate(message.size() - 1);

            qnWarning("Error in %1: %2 (%3).", function, status, message);
            LocalFree(buffer);
        } else {
            qnWarning("Error in %1: %2.", function, status);
        }

        return status;
    }

    QString perfName(DWORD index) {
        DWORD size = 0;
        PDH_STATUS status = PdhLookupPerfNameByIndexW(NULL, index, NULL, &size);
        if(status != PDH_MORE_DATA && status != ERROR_SUCCESS) {
            checkError("PdhLookupPerfNameByIndexW", status);
            return QString();
        }

        QByteArray buffer;
        buffer.resize(size * sizeof(WCHAR));
        if(INVOKE(PdhLookupPerfNameByIndexW(NULL, index, reinterpret_cast<LPWSTR>(buffer.data()), &size)) != ERROR_SUCCESS)
            return QString();

        return QString::fromWCharArray(reinterpret_cast<LPCWSTR>(buffer.constData()));
    }

    void readDiskCounterValues(PDH_HCOUNTER counter, QHash<int, HddItem> *items) {
        assert(items);
        items->clear();
        
        DWORD bufferSize = 0, itemCount = 0;

        PDH_STATUS status = PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, NULL);
        if(status != PDH_MORE_DATA && status != ERROR_SUCCESS) {
            checkError("PdhGetRawCounterArrayW", status);
            return;
        }

        /* Note that additional symbol is important here.
         * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa372642%28v=vs.85%29.aspx. */
        QByteArray buffer(bufferSize + sizeof(WCHAR), Qt::Uninitialized);
        bufferSize = buffer.size();
        itemCount = 0;

        PDH_RAW_COUNTER_ITEM_W *item = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W *>(buffer.data());
        if(INVOKE(PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, item) != ERROR_SUCCESS))
            return;

        for(int i = 0; i < itemCount; i++) {
            int id;
            LPCWSTR partitions;
            if(!parseDiskDescription(item[i].szName, &id, &partitions))
                continue; /* A '_Total' entry, disk without partitions, or simply something unexpected. */

            (*items)[id] = HddItem(Hdd(id, QLatin1String("HDD") + QString::number(id), QString::fromWCharArray(partitions)), item[i].RawValue);
        }
    }

    qreal diskCounterValue(PDH_HCOUNTER counter, const PDH_RAW_COUNTER &last, const PDH_RAW_COUNTER &current) {
        if(last.FirstValue == current.FirstValue) 
            return 0.0;
        
        PDH_FMT_COUNTERVALUE result;
        PDH_STATUS status = PdhCalculateCounterFromRawValue(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, const_cast<PDH_RAW_COUNTER *>(&current), const_cast<PDH_RAW_COUNTER *>(&last), &result);
        if(status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS) {
            checkError("PdhCalculateCounterFromRawValue", status);
            return 0.0;
        } 
        
        return result.doubleValue / 100.0;
    }

private:
    const QnWindowsMonitorPrivate *d_func() const { return this; } /* For INVOKE to work. */
    
private:
    /** Handle to PHD dll. Used to query error messages via <tt>FormatMessage</tt>. */
    HMODULE pdhLibrary;

    /** PDH query object. */
    PDH_HQUERY query;

    /** Disk time counter, <tt>'\PhysicalDisk(*)\% Disk Time'</tt>. */
    PDH_HCOUNTER diskCounter;
    
    /** Time of the last collect operation. Counter is not re-read if the
     * time passed since the last collect is small. */
    ULONGLONG lastCollectTimeMSec;

    /** Data collected from the disk time counter, in a sane format. 
     * Note that strings stored here point into the raw data buffer. */
    QHash<int, HddItem> itemByDiskId;

    /** Data collected from the disk time counter during the last collect 
     * operation. */
    QHash<int, HddItem> lastItemByDiskId;

private:
    Q_DECLARE_PUBLIC(QnWindowsMonitor);
    QnWindowsMonitor *q_ptr;
};


// -------------------------------------------------------------------------- //
// QnWindowsMonitor
// -------------------------------------------------------------------------- //
QnWindowsMonitor::QnWindowsMonitor(QObject *parent):
    base_type(parent),
    d_ptr(new QnWindowsMonitorPrivate())
{
    d_ptr->q_ptr = this;
}

QnWindowsMonitor::~QnWindowsMonitor() {
    return;
}

QList<QnPlatformMonitor::HddLoad> QnWindowsMonitor::totalHddLoad() {
    Q_D(QnWindowsMonitor);

    d->collectQuery();

    QList<QnPlatformMonitor::HddLoad> result;
    foreach(const QnWindowsMonitorPrivate::HddItem &item, d->itemByDiskId) {
        qreal load = 0.0;
        if(d->lastItemByDiskId.contains(item.hdd.id)) 
            load = d->diskCounterValue(d->diskCounter, d->lastItemByDiskId[item.hdd.id].counter, item.counter);

        result.push_back(HddLoad(item.hdd, load));
    }
    return result;
}

#endif // Q_OS_WIN
