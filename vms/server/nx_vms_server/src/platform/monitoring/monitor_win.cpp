#include "monitor_win.h"

#include <array>
#include <cassert>

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QVector>

#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
#include <nx/utils/file_system.h>
#include <utils/common/warnings.h>

#define NOMINMAX
#include <Windows.h>
#include <iphlpapi.h>
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
        bool hasName = (pos != NULL);
        if (hasName) {
            while(*pos == L' ')
                pos++;
            if(*pos == L'\0')
                return false;
        }

        int localId = _wtoi(description);
        if(localId == 0 && description[0] != L'0')
            return false;

        if(partitions) {
            if (hasName)
                *partitions = pos;
            else
                *partitions = description;
        }
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

    typedef nx::vms::server::PlatformMonitor::Hdd Hdd;

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
        lastCollectTimeMSec(0),
        m_networkStatCalcCounter()
    {
        pdhLibrary = LoadLibraryW(L"pdh.dll");
        if(!pdhLibrary)
            checkError("LoadLibrary", GetLastError());
        m_networkStatTimer.restart();
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

        /* Note that we can't use GetTickCount64 since it doesn't exist under XP.
         * GetTickCount wraps every ~50 days, but for this check the wrap is irrelevant. */
        ULONGLONG timeMSec = GetTickCount();
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
        NX_ASSERT(expression);

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
        NX_ASSERT(items);
        items->clear();

        DWORD bufferSize = 0, itemCount = 0;

        PDH_STATUS status = PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, NULL);
        if(status != PDH_MORE_DATA && status != ERROR_SUCCESS) {
            checkError("PdhGetRawCounterArrayW", status);
            return;
        }

        /* Note that additional symbol is important here.
         * See http://msdn.microsoft.com/en-us/library/windows/desktop/aa372642%28v=vs.85%29.aspx. */
        QByteArray buffer(bufferSize + sizeof(WCHAR), '\0');
        bufferSize = buffer.size();
        itemCount = 0;

        PDH_RAW_COUNTER_ITEM_W *item = reinterpret_cast<PDH_RAW_COUNTER_ITEM_W *>(buffer.data());
        if(INVOKE(PdhGetRawCounterArrayW(counter, &bufferSize, &itemCount, item) != ERROR_SUCCESS))
            return;

        for(int i = 0; i < (int) itemCount; i++) {
            int id;
            LPCWSTR partitions;
            if(!parseDiskDescription(item[i].szName, &id, &partitions))
                continue; /* A '_Total' entry or something unexpected. */

            Hdd hdd(id, QLatin1String("HDD") + QString::number(id), QString::fromWCharArray(partitions));

            /* Fix partitions name for the mounted-into-folder drives. */
            if (!hdd.partitions.contains(L':'))
                hdd.partitions = hdd.name;

            (*items)[id] = HddItem(hdd,  item[i].RawValue);
        }
    }

    qreal diskCounterValue(PDH_HCOUNTER counter, const PDH_RAW_COUNTER &last, const PDH_RAW_COUNTER &current) {
        if(last.FirstValue == current.FirstValue)
            return 0.0;

        PDH_FMT_COUNTERVALUE result;
        PDH_STATUS status = PdhCalculateCounterFromRawValue(
            counter,
            PDH_FMT_DOUBLE /*| PDH_FMT_NOCAP100*/,  //TODO #ak disk usage can be greater then 100% somehow. Maybe, disk can do some I/O concurrently
            const_cast<PDH_RAW_COUNTER *>(&current),
            const_cast<PDH_RAW_COUNTER *>(&last),
            &result);
        if(status != PDH_CSTATUS_NEW_DATA && status != ERROR_SUCCESS) {
            checkError("PdhCalculateCounterFromRawValue", status);
            return 0.0;
        }

        return result.doubleValue / 100.0;
    }

    void recalcNetworkStatistics()
    {
        static const size_t MS_PER_SEC = 1000;

        if( m_mibIfTableBuffer.size() == 0 )
            m_mibIfTableBuffer.resize( 256 );

        DWORD resultCode = NO_ERROR;
        MIB_IFTABLE* networkInterfaceTable = NULL;
        for( int i = 0; i < 2; ++i )
        {
            ULONG bufSize = (ULONG)m_mibIfTableBuffer.size();
            networkInterfaceTable = reinterpret_cast<MIB_IFTABLE*>(m_mibIfTableBuffer.data());
            resultCode = GetIfTable( networkInterfaceTable, &bufSize, TRUE );
            if( resultCode == ERROR_INSUFFICIENT_BUFFER )
            {
                m_mibIfTableBuffer.resize( bufSize );
                continue;
            }
            break;
        }

        if( resultCode != NO_ERROR )
            return;

        ++m_networkStatCalcCounter;

        const qint64 currentClock = m_networkStatTimer.elapsed();
        for( size_t i = 0; i < networkInterfaceTable->dwNumEntries; ++i )
        {
            const MIB_IFROW& ifInfo = networkInterfaceTable->table[i];
            if( ifInfo.dwPhysAddrLen == 0 )
                continue;
            if( !(ifInfo.dwType & IF_TYPE_ETHERNET_CSMACD) )
                continue;
            if( ifInfo.dwOperStatus != IF_OPER_STATUS_CONNECTED && ifInfo.dwOperStatus != IF_OPER_STATUS_OPERATIONAL )
                continue;

            const QByteArray physicalAddress( reinterpret_cast<const char*>(ifInfo.bPhysAddr), ifInfo.dwPhysAddrLen );

            std::pair<std::map<QByteArray, NetworkInterfaceStatData>::iterator, bool>
                p = m_interfaceLoadByMAC.emplace( physicalAddress, NetworkInterfaceStatData() );
            if( p.second )
            {
                p.first->second.load.interfaceName = ( ifInfo.dwDescrLen == 0 )
                        ? QString( QLatin1String( "Unnamed" ) )
                        : QString::fromLocal8Bit( reinterpret_cast<const char*>(ifInfo.bDescr),
                                                  ifInfo.dwDescrLen - 1 );

                p.first->second.load.macAddress = nx::utils::MacAddress( physicalAddress );
                p.first->second.load.type = nx::vms::server::PlatformMonitor::PhysicalInterface;
                p.first->second.load.bytesPerSecMax = ifInfo.dwSpeed / CHAR_BIT;
                p.first->second.prevMeasureClock = currentClock;
                p.first->second.inOctets = ifInfo.dwInOctets;
                p.first->second.outOctets = ifInfo.dwOutOctets;
            }
            NetworkInterfaceStatData& intfLoad = p.first->second;
            intfLoad.networkStatCalcCounter = m_networkStatCalcCounter;
            if( intfLoad.prevMeasureClock == currentClock )
                continue;   //not calculating load
            const qint64 msPassed = currentClock - intfLoad.prevMeasureClock;
            intfLoad.load.bytesPerSecIn = p.first->second.load.bytesPerSecIn * 0.3 + ((ifInfo.dwInOctets - (DWORD)p.first->second.inOctets) * MS_PER_SEC / msPassed) * 0.7;
            intfLoad.load.bytesPerSecOut = p.first->second.load.bytesPerSecOut * 0.3 + ((ifInfo.dwOutOctets - (DWORD)p.first->second.outOctets) * MS_PER_SEC / msPassed) * 0.7;

            p.first->second.inOctets = ifInfo.dwInOctets;
            p.first->second.outOctets = ifInfo.dwOutOctets;
            p.first->second.prevMeasureClock = currentClock;
        }

        //removing disabled interface
        for( auto it = m_interfaceLoadByMAC.begin(); it != m_interfaceLoadByMAC.end(); )
        {
            if( it->second.networkStatCalcCounter < m_networkStatCalcCounter )
                it = m_interfaceLoadByMAC.erase(it);
            else
                ++it;
        }
    }

    QList<nx::vms::server::PlatformMonitor::NetworkLoad> networkInterfacelLoadData()
    {
        QList<nx::vms::server::PlatformMonitor::NetworkLoad> loadData;
        for( const std::pair<QByteArray, NetworkInterfaceStatData>& ifStatData: m_interfaceLoadByMAC )
            loadData.push_back( ifStatData.second.load );
        return loadData;
    }

private:
    const QnWindowsMonitorPrivate *d_func() const { return this; } /* For INVOKE to work. */

    struct NetworkInterfaceStatData
    {
        nx::vms::server::PlatformMonitor::NetworkLoad load;
        ULONG64 inOctets;
        ULONG64 outOctets;
        qint64 prevMeasureClock;
        size_t networkStatCalcCounter;

        NetworkInterfaceStatData()
        :
            inOctets(0),
            outOctets(0),
            prevMeasureClock(-1),
            networkStatCalcCounter(0)
        {
        }
    };

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

    std::map<QByteArray, NetworkInterfaceStatData> m_interfaceLoadByMAC;
    QElapsedTimer m_networkStatTimer;
    size_t m_networkStatCalcCounter;
    std::vector<char> m_mibIfTableBuffer;

    Q_DECLARE_PUBLIC(QnWindowsMonitor);
    QnWindowsMonitor *q_ptr;
};


// -------------------------------------------------------------------------- //
// QnSysDependentMonitor
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

namespace {

class WindowsDrivesInfoFetcher
{
public:
    QList<nx::vms::server::PlatformMonitor::PartitionSpace> getInfoList()
    {
        if (!fillDriveNamesBuf())
            return m_infoList;

        QString driveString;
        while (getNextDriveString(&driveString))
            processDrive(driveString);

        return m_infoList;
    }

private:
    class DriveInfo
    {
    public:
        explicit DriveInfo(const QString& driveName): m_drivePath(driveName)
        {
            if (!openHandle())
            {
                NX_WARNING(this, lm("Failed to open handle for drive %1").arg(driveName));
                return;
            }

            m_partition.devName = m_drivePath;
            m_partition.path = m_drivePath;
            if (!retrivePartitionType())
            {
                NX_WARNING(
                    this,
                    lm("Failed to retrive partition type for drive %1").arg(driveName));
                return;
            }

            if (isRemovable() && !isMediaOk())
            {
                NX_VERBOSE(
                    this,
                    lm("Media is not inserted or is not writable for removable drive %1")
                        .arg(driveName));
            }

            if (!retriveSpaceInfo())
            {
                NX_VERBOSE(
                    this,
                    lm("Failed to retrieve partition space information for drive %1")
                        .arg(driveName));
                return;
            }

            m_ok = true;
        }

        ~DriveInfo()
        {
            CloseHandle(m_driveHandle);
        }

        bool ok() const { return m_ok; }
        nx::vms::server::PlatformMonitor::PartitionSpace partition() const { return m_partition; }
    private:
        QString m_drivePath;
        HANDLE m_driveHandle = INVALID_HANDLE_VALUE;
        bool m_ok = false;
        nx::vms::server::PlatformMonitor::PartitionSpace m_partition;

        bool openHandle()
        {
            QString driveSysString = lit("\\\\.\\%1:").arg(m_drivePath[0]);
            m_driveHandle = CreateFile(
                reinterpret_cast<LPCWSTR>(driveSysString.data()),
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);
            return m_driveHandle != INVALID_HANDLE_VALUE;
        }

        bool retrivePartitionType()
        {
            const UINT systemDriveType =
                GetDriveType(reinterpret_cast<LPCWSTR>(m_partition.path.data()));
            switch (systemDriveType)
            {
                case DRIVE_NO_ROOT_DIR:
                    return false;
                case DRIVE_REMOVABLE:
                    m_partition.type = nx::vms::server::PlatformMonitor::RemovableDiskPartition;
                    break;
                case DRIVE_FIXED:
                    m_partition.type = nx::vms::server::PlatformMonitor::LocalDiskPartition;
                    break;
                case DRIVE_REMOTE:
                    m_partition.type = nx::vms::server::PlatformMonitor::NetworkPartition;
                    break;
                case DRIVE_CDROM:
                    m_partition.type = nx::vms::server::PlatformMonitor::OpticalDiskPartition;
                    break;
                case DRIVE_RAMDISK:
                    m_partition.type = nx::vms::server::PlatformMonitor::RamDiskPartition;
                    break;
                case DRIVE_UNKNOWN:
                default:
                    m_partition.type = nx::vms::server::PlatformMonitor::UnknownPartition;
                    break;
            }
            return true;
        }

        bool isRemovable() const
        {
            return m_partition.type == nx::vms::server::PlatformMonitor::RemovableDiskPartition;
        }

        bool isMediaOk() const
        {
            return isInserted() && isWritable();
        }

        bool isInserted() const
        {
            DWORD bytesReturned;
            return DeviceIoControl(
                m_driveHandle,
                IOCTL_STORAGE_CHECK_VERIFY2,
                NULL, 0,
                NULL, 0,
                &bytesReturned,
                NULL);
        }

        bool isWritable() const
        {
            DWORD bytesReturned;
            return DeviceIoControl(
                m_driveHandle,
                IOCTL_DISK_IS_WRITABLE,
                NULL, 0,
                NULL, 0,
                &bytesReturned,
                NULL);
        }

        bool retriveSpaceInfo()
        {
            quint64 freeBytesAvailableToCaller = -1;
            quint64 totalNumberOfBytes = -1;
            quint64 totalNumberOfFreeBytes = -1;
            if (!GetDiskFreeSpaceEx(
                reinterpret_cast<LPCWSTR>(m_drivePath.constData()),
                reinterpret_cast<PULARGE_INTEGER>(&freeBytesAvailableToCaller),
                reinterpret_cast<PULARGE_INTEGER>(&totalNumberOfBytes),
                reinterpret_cast<PULARGE_INTEGER>(&totalNumberOfFreeBytes)))
            {
                return false;
            }
            m_partition.sizeBytes = totalNumberOfBytes;
            m_partition.freeBytes = totalNumberOfFreeBytes;
            return true;
        }
    };

    QList<nx::vms::server::PlatformMonitor::PartitionSpace> m_infoList;
    std::array<WCHAR, 512> m_driveNamesBuf;
    const WCHAR* m_bufPtr = nullptr;

    bool fillDriveNamesBuf()
    {
        if (!GetLogicalDriveStringsW(
                static_cast<DWORD>(m_driveNamesBuf.size()),
                m_driveNamesBuf.data()))
        {
            NX_ERROR(this, "GetLogicalDriveStringsW failed");
            return false;
        }

        m_bufPtr = m_driveNamesBuf.data();
        return true;
    }

    bool getNextDriveString(QString* driveString)
    {
        if (*m_bufPtr == L'\0')
            return false;

        *driveString = QString::fromUtf16(reinterpret_cast<const ushort*>(m_bufPtr));
        m_bufPtr += driveString->length() + 1;

        return true;
    }

    void processDrive(const QString& driveName)
    {
        DriveInfo driveInfo(driveName);
        if (!driveInfo.ok())
            return;
        m_infoList.append(driveInfo.partition());
    }
};

} // <anonymous>

QList<nx::vms::server::PlatformMonitor::PartitionSpace> QnWindowsMonitor::totalPartitionSpaceInfo()
{
    return WindowsDrivesInfoFetcher().getInfoList();
}

QList<nx::vms::server::PlatformMonitor::HddLoad> QnWindowsMonitor::totalHddLoad() {
    Q_D(QnWindowsMonitor);

    d->collectQuery();

    QList<nx::vms::server::PlatformMonitor::HddLoad> result;
    for(const QnWindowsMonitorPrivate::HddItem &item: d->itemByDiskId) {
        qreal load = 0.0;
        if(d->lastItemByDiskId.contains(item.hdd.id))
            load = d->diskCounterValue(d->diskCounter, d->lastItemByDiskId[item.hdd.id].counter, item.counter);

        result.push_back(HddLoad(item.hdd, load));
    }
    return result;
}

QList<nx::vms::server::PlatformMonitor::NetworkLoad> QnWindowsMonitor::totalNetworkLoad()
{
    Q_D(QnWindowsMonitor);

    d->recalcNetworkStatistics();

    return d->networkInterfacelLoadData();
}
