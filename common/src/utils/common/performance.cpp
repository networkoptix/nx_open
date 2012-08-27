#include "performance.h"

#include <map>
#include <set>
#include <string>
#include <iostream>

#include <QtCore/QtGlobal>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include "warnings.h"


#if defined(Q_OS_WIN)
#   define NOMINMAX
#   define _WIN32_DCOM
#   include <Windows.h>
#   include <comdef.h>
#   include <Wbemidl.h>
#   include <QLibrary>
#   pragma comment(lib, "wbemuuid.lib")
#elif defined(Q_OS_LINUX)
#   include <time.h>
#   include <signal.h>
#   include <errno.h>
#   define SIG_CODE SIGRTMIN
#endif


/** statistics usage update timer, in seconds. */
#define STATISTICS_USAGE_REFRESH 1

using namespace std;

// -------------------------------------------------------------------------- //
// CPU Usage
// -------------------------------------------------------------------------- //
#if defined(Q_OS_WIN)

namespace {

    class CpuUsageRefresher {
    public:
        CpuUsageRefresher():
           m_locator(NULL),
           m_service(NULL),
           m_processCpu(0),
           m_totalIdle(0),
           m_timeStamp(0),
           m_usage(0),
           m_totalUsage(0){
            m_initialized = init();
        }

        ~CpuUsageRefresher() {
            if(m_initialized) {
                m_service->Release(); 
                m_locator->Release();     
                CoUninitialize();
            }
            if(m_timer != 0) {
                KillTimer(NULL, m_timer); 
                m_timer = 0;
            }
        }

        uint usage() {
            return m_usage;
        }

        uint totalUsage(){
            return m_totalUsage;
        }

        bool hddUsage(QList<int> *output){
            QMutexLocker lk( &m_mutex );

            output->clear();
            foreach(int item, m_hddUsage)
                output->append(item);
            return !m_hddUsage.isEmpty();
        }

    private:
        static VOID CALLBACK timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/);

        qulonglong getWbemLongLong(IWbemClassObject *pclsObj, LPCWSTR wszName, qulonglong def = 0){
            VARIANT vtProc;
            pclsObj->Get(wszName,0, &vtProc, 0, 0);
            qulonglong result = def;
            if (vtProc.vt == VT_BSTR){
                QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal));
                result = qstr.toULongLong();
            }
            VariantClear(&vtProc);
            return result;
        }

        QString getWbemString(IWbemClassObject *pclsObj, LPCWSTR wszName){
            VARIANT vtProc;
            pclsObj->Get(wszName,0, &vtProc, 0, 0);
            QString result = QString();
            if (vtProc.vt == VT_BSTR){
                QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal));
                result = qstr;
            }
            VariantClear(&vtProc);
            return result;
        }

        int getWbemInt(IWbemClassObject *pclsObj, LPCWSTR wszName, int def = -1){
            VARIANT vtProc;
            pclsObj->Get(wszName,0, &vtProc, 0, 0);
            int result = def;
            if (vtProc.vt == VT_I4)
                result = vtProc.intVal;
            VariantClear(&vtProc);
            return result;
        }
    
        quint64 enumerateWbem(QString queryStr, LPCWSTR value){
            qulonglong result = 0;
            IEnumWbemClassObject* pEnumerator = query(queryStr.toLocal8Bit().data());
            if (pEnumerator){
                IWbemClassObject *pclsObj;
                ULONG uReturn = 0;
                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                while (uReturn != 0)
                {
                    qulonglong data = getWbemLongLong(pclsObj, value);
                    result+= data;
                    pclsObj->Release();
                    pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                }
                pEnumerator->Release();
            }
            return result;
        }

        QList<quint64> *enumerateWbemSeparate(QString queryStr, LPCWSTR value, QList<quint64> *dataList){
            IEnumWbemClassObject* pEnumerator = query(queryStr.toLocal8Bit().data());
            if (pEnumerator){
                IWbemClassObject *pclsObj;
                ULONG uReturn = 0;
                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                while (uReturn != 0)
                {
                    quint64 data = getWbemLongLong(pclsObj, value);
                    dataList->append(data);
                    pclsObj->Release();
                    pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                }
                pEnumerator->Release();
            }
            return dataList;
        }

        void refresh() {
            QMutexLocker lk( &m_mutex );

            const quint64 PERCENT_CAP = 100;

            quint64 cpu = enumerateWbem(
                QString::fromLatin1("SELECT * FROM Win32_PerfRawData_PerfProc_Process WHERE IdProcess = %1").arg(QCoreApplication::applicationPid()),
                L"PercentProcessorTime");
            quint64 timeStamp = enumerateWbem(
                QString::fromLatin1("SELECT * FROM Win32_PerfRawData_PerfProc_Process WHERE IdProcess = %1").arg(QCoreApplication::applicationPid()),
                L"Timestamp_Sys100NS");
            quint64 idle = enumerateWbem(
                QString::fromLatin1("SELECT * FROM Win32_PerfRawData_PerfProc_Process WHERE Name = \"Idle\""),
                L"PercentProcessorTime");
             QList<quint64> *hdds = enumerateWbemSeparate(
                QString::fromLatin1("SELECT * FROM Win32_PerfRawData_PerfDisk_PhysicalDisk WHERE Name !=\"_Total\" AND PercentDiskTime > 0"),
                L"PercentIdleTime",
                new QList<quint64>);

            if (m_timeStamp && timeStamp){
                qulonglong timespan = (timeStamp - m_timeStamp);
                m_hddUsage.clear();
                for (int i = 0; i < qMin(hdds->count(), m_rawHddUsageIdle.count()); i++){
                    m_hddUsage.append(PERCENT_CAP - qMin(PERCENT_CAP, (hdds->at(i) - m_rawHddUsageIdle.at(i)) * PERCENT_CAP / timespan ));
                }

                timespan *= QnPerformance::cpuCoreCount(); // cpu counters are relayed on this coeff
                m_totalUsage = PERCENT_CAP - qMin(PERCENT_CAP, ((idle - m_totalIdle) * PERCENT_CAP) / timespan);
                m_usage = qMin(PERCENT_CAP, ((cpu - m_processCpu) * 100) / timespan);
            }

            m_rawHddUsageIdle.clear();
            for(int i = 0; i < hdds->count(); i++)
                m_rawHddUsageIdle.append(hdds->at(i));
            delete hdds;

            m_totalIdle = idle;
            m_processCpu = cpu;
            m_timeStamp = timeStamp;
        }

        bool init() {
            if (QSysInfo::windowsVersion() <= QSysInfo::WV_2000) {
                qnWarning("Windows version too low to use WMI");
                return false;
            }

            HRESULT hres;

            // Step 1: --------------------------------------------------
            // Initialize COM. ------------------------------------------
            hres =  CoInitializeEx(0, COINIT_APARTMENTTHREADED); 
            if (FAILED(hres))
            {
                qnWarning("WMI: Failed to initialize COM library. Error code = 0x%1", QString::number(hres, 16));
                return false;
            }

            // Step 2: --------------------------------------------------
            // Set general COM security levels --------------------------
            hres =  CoInitializeSecurity(
                NULL, 
                -1,                          // COM authentication
                NULL,                        // Authentication services
                NULL,                        // Reserved
                RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
                RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
                NULL,                        // Authentication info
                EOAC_NONE,                   // Additional capabilities 
                NULL                         // Reserved
                );

            if (FAILED(hres))
            {
                qnWarning("WMI: Failed to initialize security. Error code = 0x%1", QString::number(hres, 16));
                CoUninitialize();
                return false;                    
            }

            // Step 3: ---------------------------------------------------
            // Obtain the initial locator to WMI -------------------------
            hres = CoCreateInstance(
                CLSID_WbemLocator,             
                0, 
                CLSCTX_INPROC_SERVER, 
                IID_IWbemLocator, (LPVOID *) &m_locator);

            if (FAILED(hres))
            {
                qnWarning("WMI: Failed to create IWbemLocator object. Err code = 0x%1", QString::number(hres, 16));
                CoUninitialize();
                return false;                
            }

            // Step 4: -----------------------------------------------------
            // Connect to WMI through the IWbemLocator::ConnectServer method


            // Connect to the root\cimv2 namespace with
            // the current user and obtain pointer pSvc
            // to make IWbemServices calls.
            hres = m_locator->ConnectServer(
                _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
                NULL,                    // User name. NULL = current user
                NULL,                    // User password. NULL = current
                0,                       // Locale. NULL indicates current
                NULL,                    // Security flags.
                0,                       // Authority (e.g. Kerberos)
                0,                       // Context object 
                &m_service               // pointer to IWbemServices proxy
                );

            if (FAILED(hres))
            {
                qnWarning("WMI: Could not connect. Error code = 0x%1", QString::number(hres, 16));
                m_locator->Release();
                CoUninitialize();
                return false;
            }

            // Step 5: --------------------------------------------------
            // Set security levels on the proxy -------------------------
            hres = CoSetProxyBlanket(
                m_service,                   // Indicates the proxy to set
                RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
                RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
                NULL,                        // Server principal name 
                RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
                RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
                NULL,                        // client identity
                EOAC_NONE                    // proxy capabilities 
                );

            if (FAILED(hres))
            {
                qnWarning("WMI: Could not set proxy blanket. Error code = 0x%1", QString::number(hres, 16));
                m_service->Release();
                m_locator->Release();     
                CoUninitialize();
                return false;
            }
            m_timer = SetTimer(0, 0, STATISTICS_USAGE_REFRESH * 1000, timerCallback);
            return true;
        }

        
        IEnumWbemClassObject *query(char *query_str) {
            IEnumWbemClassObject* enumerator = NULL;
            HRESULT hres;
            hres = m_service->ExecQuery(
                bstr_t("WQL"), 
                bstr_t(query_str),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
                NULL,
                &enumerator);

            if (FAILED(hres))
            {
                qnWarning("WMI: Query failed. Error code = 0x%1", QString::number(hres, 16));
                return NULL;
            }
            return enumerator;
        }

    private:
        QMutex m_mutex;

        /** Flag of successful initialization. */
        bool m_initialized;

        /** WMI namespace locator interface. */
        IWbemLocator *m_locator;

        /** WMI services provider interface. */ 
        IWbemServices *m_service;

        /** Previous value of raw WMI PercentProcessorTime stat for our process. */
        quint64 m_processCpu;

        /** Previous value of raw WMI PercentProcessorTime stat for the IDLE process. */
        quint64 m_totalIdle;

        /** Previous value of raw WMI Timestamp_Sys100NS stat for our process. */
        quint64 m_timeStamp;

        /** Previous values of raw WMI PercentIdleTime stat for all drives. */
        QList<quint64> m_rawHddUsageIdle;

        /** Processor usage percent by our process for the last STATISTICS_USAGE_REFRESH seconds. */
        uint m_usage;

        /** Total processor usage percent for the last STATISTICS_USAGE_REFRESH seconds. */
        uint m_totalUsage;

        /** Total hdd usage percent for all drives for the last STATISTICS_USAGE_REFRESH seconds. */
        QList<int> m_hddUsage;

        /** System timer for processor usage calculating. */
        UINT_PTR m_timer;
    };

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance);

    VOID CALLBACK CpuUsageRefresher::timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
        refresherInstance()->refresh();
    }

} // anonymous namespace

#elif defined(Q_OS_LINUX)

namespace{
// very useful function, I'd move it to any shared linux util class
    static QString getSystemOutput(QString cmds)
    {
        QStringList list = cmds.split(QLatin1Char('|'));
        QStringListIterator iter(list);

        if (!iter.hasNext())
            return QString();
        QString cmd = iter.next();
        QProcess *prev = new QProcess();

        while(iter.hasNext()){
            QProcess *next = new QProcess();
            prev->setStandardOutputProcess(next);
            prev->start(cmd);
            if (!prev->waitForStarted())
                return QString();
            if (!prev->waitForFinished())
                return QString();

            prev = next;
            cmd = iter.next();
        }
        prev->start(cmd);
        if (!prev->waitForStarted())
            return QString();
        if (!prev->waitForFinished())
            return QString();

        return QString::fromUtf8(prev->readAll());
    }

    static void timerCallback(int sig, siginfo_t *si, void *uc);

    class CpuUsageRefresher {
    public:
        CpuUsageRefresher() {
            m_cpu = 0;
            m_processCpu = 0;
            m_busyCpu = 0;
            m_usage = 0;
            m_totalUsage = 0;
            m_timer = 0;
            m_initialized = init();
            memset( &m_prevClock, 0, sizeof(m_prevClock) );
            m_prevPartitionListReadTime = 0;
        }

        ~CpuUsageRefresher() {
            if (m_timer)
                timer_delete(m_timer);
        }

        uint usage() const {
            return m_usage;
        }

        uint totalUsage() const {
            return m_totalUsage;
        }

        void** getTimerId() {
            return &m_timer;
        }

        void refresh() {
            qulonglong cpu, busy;
            getCpu(busy, cpu);
            qulonglong process_cpu = getProcessCpu();
            if (process_cpu && m_processCpu){
                m_usage = ((process_cpu - m_processCpu) * 100) / (cpu - m_cpu);
            }
            if (busy && m_processCpu){
                m_totalUsage = ((busy - m_busyCpu) * 100) / (cpu - m_cpu);
            }
            m_cpu = cpu;
            m_processCpu = process_cpu;
            m_busyCpu = busy;

            calcDiskUsage();
        }

        bool hddUsage( QList<int>* output ) const
        {
            QMutexLocker lk( &m_mutex );

            output->clear();
            for( map<pair<int, int>, DiskStatContext>::const_iterator
                 it = m_devNameToStat.begin();
                 it != m_devNameToStat.end();
                 ++it )
            {
                output->append( it->second.prevStat.diskUtilizationPercent );
            }
            return !output->isEmpty();
        }

    private:
        mutable QMutex m_mutex;

        bool init() {
            /* Establish handler for timer signal */
            struct sigaction sa;
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = timerCallback;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIG_CODE, &sa, NULL) == -1){
                qnWarning("Establishing handler error %1", errno);
                return false;
            }

            /* Create the timer */
            struct sigevent sev;
            sev.sigev_notify = SIGEV_SIGNAL;
            sev.sigev_signo = SIG_CODE;
            sev.sigev_value.sival_ptr = getTimerId();
            if (timer_create(CLOCK_REALTIME, &sev, &m_timer) == -1){
                qnWarning("Creating timer error %1", errno);
                return false;
            }

            /* Start the timer */
            struct itimerspec its;
            its.it_value.tv_sec = STATISTICS_USAGE_REFRESH;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_sec = its.it_value.tv_sec;
            its.it_interval.tv_nsec = its.it_value.tv_nsec;

            if (timer_settime(m_timer, 0, &its, NULL) == -1){
                qnWarning("Starting timer error %1", errno);
                return false;
            }
            return true;
        }

        /** Calculate total cpu jiffies */
        void getCpu(qulonglong &busy, qulonglong &total){
             busy = 0;
            total = 0;

            QString proc_stat = getSystemOutput(QLatin1String("cat /proc/stat | grep \"cpu \"")).mid(5);
            QStringList list = proc_stat.split(QLatin1Char(' '), QString::SkipEmptyParts);
            QStringListIterator iter(list);
            int counter = 0;
            while (iter.hasNext()){
                qulonglong value = iter.next().toULongLong();
                total += value;
                /** user,nice,system = the first 3 values */
                if (counter < 3)
                    busy += value;
                counter++;
            }
        }

        qulonglong getProcessCpu() {
            QString request = QString(QLatin1String("cat /proc/%1/stat")).arg(QCoreApplication::applicationPid());
            QStringList proc_stat = getSystemOutput(request).split(QLatin1Char(' '), QString::SkipEmptyParts);
            if (proc_stat.count() > 14){
                return (proc_stat[13]).toULongLong() +
                        proc_stat[14].toULongLong();
            }
            else{
                qWarning("Linux: proc stat is absent");
                return 0;
            }
        }

        //!This structure is read from /proc/diskstat
        struct DiskStatSys
        {
            int major_num;
            int minor_num;
            char device_name[FILENAME_MAX];
            unsigned int num_reads;
            unsigned int reads_merged;
            unsigned int sectors_read;
            unsigned int tot_read_ms;
            unsigned int num_writes;
            unsigned int writes_merged;
            unsigned int sectors_written;
            unsigned int tot_write_ms;
            unsigned int cur_io_cnt;
            unsigned int tot_io_ms;
            unsigned int io_weighted_ms;
        };

        class DiskStat
        {
        public:
            int major_num;
            int minor_num;
            std::string deviceName;
            unsigned int diskUtilizationPercent;

            DiskStat()
            :
                major_num(0),
                minor_num(0),
                diskUtilizationPercent(0)
            {
            }
        };

        class DiskStatContext
        {
        public:
            DiskStatSys prevSysStat;
            DiskStat prevStat;
            bool initialized;

            DiskStatContext()
            :
                initialized( false )
            {
                memset( &prevSysStat, 0, sizeof(prevSysStat) );
            }
        };

        //!Timeout (in seconds) during which partition list expires (needed to detect mounts/umounts)
        static const time_t PARTITION_LIST_EXPIRE_TIMEOUT_SEC = 60;
        //!Disk usage is evaluated as average on \a APPROXIMATION_VALUES_NUMBER prev values
        static const unsigned int APPROXIMATION_VALUES_NUMBER = 3;

        void calcDiskUsage()
        {
            static const size_t MAX_LINE_LENGTH = 256;
            char line[MAX_LINE_LENGTH];

            char devName[MAX_LINE_LENGTH];
            memset( devName, 0, sizeof(devName) );
            const time_t curTime = ::time( NULL );
            if( m_partitions.empty() || curTime - m_prevPartitionListReadTime > PARTITION_LIST_EXPIRE_TIMEOUT_SEC )
            {
//                //using all mount points
//                FILE* mntF = popen( "mount | grep ^/dev/", "r" );
//                if( mntF == NULL )
//                {
//                    //failed to get partition list
//                }
//                else
//                {
//                    m_partitions.clear();
//                    char PARTITION_NAME[1024];
//                    while( fgets(PARTITION_NAME, sizeof(PARTITION_NAME)-1, mntF) != NULL)
//                    {
//                        char* sepPos = strchr( PARTITION_NAME, ' ' );
//                        if( sepPos == NULL || sepPos == PARTITION_NAME )
//                            continue;
//                        //searching device name
//                        const char* shortNameStart = PARTITION_NAME;
//                        for( const char*
//                             curPos = PARTITION_NAME;
//                             curPos < sepPos && curPos != NULL;
//                             curPos = strchr( curPos+1, '/' ) )
//                        {
//                            shortNameStart = curPos;
//                        }

//                        m_partitions.insert( string( shortNameStart+1, sepPos-shortNameStart-1 ) );
//                    }

//                    pclose( mntF );
//                    m_prevPartitionListReadTime = curTime;
//                }


                //using all disk drives
                unsigned int majorNumber = 0, minorNumber = 0, numBlocks = 0;
                //reading partition names
                FILE* partitionsFile = fopen( "/proc/partitions", "r" );
                if( !partitionsFile )
                    return;
                for( int i = 0; fgets( line, MAX_LINE_LENGTH, partitionsFile ) != NULL; ++i )
                {
                    if( i == 0 ||   //skipping header
                            sscanf( line, "%u %u %u %s", &majorNumber, &minorNumber, &numBlocks, devName ) != 4 )  //skipping unrecognized line
                        continue;
                    string devNameStr( devName );
                    if( devNameStr.empty() || isdigit(devNameStr[devNameStr.size()-1]) )
                        continue;   //not a physical drive
                    m_partitions.insert( devNameStr );
                }
                fclose( partitionsFile );

                //TODO/IMPL read network drives
            }

            DiskStatSys curDiskStat;
            memset( &curDiskStat, 0, sizeof(curDiskStat) );

            struct timespec curClock;
            memset( &curClock, 0, sizeof(curClock) );
            clock_gettime( CLOCK_MONOTONIC, &curClock );
            if( !m_prevClock.tv_sec )   //m_prevClock can overflow and become 0. In this case we will skip one calculation. Nothing serious...
            {
                m_prevClock = curClock;
                return; //first time doing nothing
            }

            //reading current disk statistics
            FILE* diskstatFile = fopen( "/proc/diskstats", "r" );
            if( !diskstatFile )
                return;

            unsigned int clockElapsed = (curClock.tv_sec - m_prevClock.tv_sec) * 1000;
            if( curClock.tv_nsec >= m_prevClock.tv_nsec )
                clockElapsed += (curClock.tv_nsec - m_prevClock.tv_nsec) / 1000000;
            else
                clockElapsed -= (m_prevClock.tv_nsec - curClock.tv_nsec) / 1000000;
            if( clockElapsed == 0 )
                return; //there's no sense to calculate anything
            m_prevClock = curClock;

            while( fgets( line, MAX_LINE_LENGTH, diskstatFile ) != NULL )
            {
                memset( curDiskStat.device_name, 0, sizeof(curDiskStat.device_name) );
                if( sscanf( line, "%u %u %s %u %u %u %u %u %u %u %u %u %u %u",
                        &curDiskStat.major_num,
                        &curDiskStat.minor_num,
                        curDiskStat.device_name,
                        &curDiskStat.num_reads,
                        &curDiskStat.reads_merged,
                        &curDiskStat.sectors_read,
                        &curDiskStat.tot_read_ms,
                        &curDiskStat.num_writes,
                        &curDiskStat.writes_merged,
                        &curDiskStat.sectors_written,
                        &curDiskStat.tot_write_ms,
                        &curDiskStat.cur_io_cnt,
                        &curDiskStat.tot_io_ms,
                        &curDiskStat.io_weighted_ms ) != 14 )
                {
                    continue;
                }

                if( m_partitions.find( curDiskStat.device_name ) == m_partitions.end() )
                    continue;   //not a partition

                QMutexLocker lk( &m_mutex );

                pair<map<pair<int, int>, DiskStatContext>::iterator, bool> p = m_devNameToStat.insert( make_pair(
                    make_pair(curDiskStat.major_num, curDiskStat.minor_num),
                    DiskStatContext() ) );

                DiskStatContext& ctx = p.first->second;
                if( p.second )
                {
                    //initializing...
                    ctx.prevStat.major_num = curDiskStat.major_num;
                    ctx.prevStat.minor_num = curDiskStat.minor_num;
                    ctx.prevStat.deviceName = curDiskStat.device_name;
                }
                else
                {
                    //calculating disk utilization
                    ctx.prevStat.diskUtilizationPercent = (curDiskStat.tot_io_ms - ctx.prevSysStat.tot_io_ms) * 100 / clockElapsed;
                    //std::cout<<ctx.prevStat.deviceName<<": "<<ctx.prevStat.diskUtilizationPercent<<"%\n";
                }
                ctx.prevSysStat = curDiskStat;
            }

            fclose( diskstatFile );
        }

        bool m_initialized;
        quint64 m_cpu;
        quint64 m_processCpu;
        quint64 m_busyCpu;
        uint m_usage;
        uint m_totalUsage;
        timer_t m_timer;
        struct timespec m_prevClock;
        //!map<pair<majorNumber, minorNumber>, statistics calculation context>
        map<pair<int, int>, DiskStatContext> m_devNameToStat;
        set<string> m_partitions;
        time_t m_prevPartitionListReadTime;
    };

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance);
    // initializer for Q_GLOBAL_STATIC singleton
    CpuUsageRefresher* dummy = refresherInstance();

    static void timerCallback(int, siginfo_t *si, void*) {
        CpuUsageRefresher *refresher = refresherInstance();
        if (si->si_value.sival_ptr == refresher->getTimerId()){
            refresher->refresh();
        }
    }
}
#endif

#ifdef Q_OS_WIN
qint64 fileTimeToNSec(const FILETIME &fileTime) {
    LARGE_INTEGER intTime;
    intTime.HighPart = fileTime.dwHighDateTime;
    intTime.LowPart = fileTime.dwLowDateTime;

    /* Convert from 100-nanoseconds to nanoseconds. */
    return intTime.QuadPart * 100;
}

qint64 estimateCpuFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    DWORD64 startCycles = __rdtsc();

    LARGE_INTEGER stop;
    QueryPerformanceCounter(&stop);
    stop.QuadPart += freq.QuadPart / 10; /* Run for 0.1 sec. */

    LARGE_INTEGER current;
    do {
        QueryPerformanceCounter(&current);
    } while (current.QuadPart < stop.QuadPart);

    DWORD64 endCycles = __rdtsc();

    return (endCycles - startCycles) * 10;
}

Q_GLOBAL_STATIC_WITH_INITIALIZER(qint64, qn_estimatedCpuFrequency, { *x = estimateCpuFrequency(); });
#endif


QString estimateCpuBrand() {
#if defined(Q_OS_WIN)
    int oCPUInfo[4] = {-1}; 
    unsigned nExIds, i =  0;
    char brandString[0x40];
    // Get the information associated with each extended ID.
    __cpuid(oCPUInfo, 0x80000000);
    nExIds = oCPUInfo[0];
    for (i=0x80000000; i<=nExIds; ++i)
    {
        __cpuid(oCPUInfo, i);
        // Interpret CPU brand string
        if  (i == 0x80000002)
            memcpy(brandString, oCPUInfo, sizeof(oCPUInfo));
        else if  (i == 0x80000003)
            memcpy(brandString + 16, oCPUInfo, sizeof(oCPUInfo));
        else if  (i == 0x80000004)
            memcpy(brandString + 32, oCPUInfo, sizeof(oCPUInfo));
    }
    return QLatin1String(brandString);
#elif defined(Q_OS_LINUX)
    // 13 - const length of 'model name : ' string with tabs - standard output of /proc/cpuinfo
    return getSystemOutput(QLatin1String("grep \"model name\" /proc/cpuinfo | head -1")).mid(13);
#else
    return QLatin1String("Unknown CPU");
#endif
}

Q_GLOBAL_STATIC_WITH_INITIALIZER(QString, qn_estimatedCpuBrand, { *x = estimateCpuBrand(); });

qint64 QnPerformance::currentThreadTimeMSecs() {
    qint64 result = currentThreadTimeNSecs();
    if(result == -1)
        return -1;
    return result / 1000000;
}

qint64 QnPerformance::currentThreadTimeNSecs() {
#ifdef Q_OS_WIN
    FILETIME userTime, kernelTime, t0, t1;
    BOOL status = GetThreadTimes(GetCurrentThread(), &t0, &t1, &kernelTime, &userTime);
    if(!SUCCEEDED(status))
        return -1;
    return fileTimeToNSec(userTime) + fileTimeToNSec(kernelTime);
#else
    return -1;
#endif
}

#ifdef Q_OS_WIN
BOOL getQueryThreadCycleTimeDefault(HANDLE , PULONG64) {
    return FALSE;
}

class PerformanceFunctions {
    typedef BOOL (*PtrQueryThreadCycleTime)(HANDLE ThreadHandle, PULONG64 CycleTime);

public:
    PerformanceFunctions() {
        QLibrary Kernel32Lib(QLatin1String("Kernel32"));    
        PtrQueryThreadCycleTime result = (PtrQueryThreadCycleTime) Kernel32Lib.resolve("QueryThreadCycleTime");
        if (result)
            queryThreadCycleTime = result;
        else
            queryThreadCycleTime = &getQueryThreadCycleTimeDefault;
    }

    PtrQueryThreadCycleTime queryThreadCycleTime;
};

Q_GLOBAL_STATIC(PerformanceFunctions, performanceInstance);
#endif // Q_OS_WIN

qint64 QnPerformance::currentThreadCycles() {
#ifdef Q_OS_WIN
    ULONG64 time;
    BOOL status = performanceInstance()->queryThreadCycleTime(GetCurrentThread(), &time);
    if(!SUCCEEDED(status))
        return -1;
    return time;
#else
    return -1;
#endif
}

qint64 QnPerformance::currentCpuFrequency() {
#ifdef Q_OS_WIN
    return *qn_estimatedCpuFrequency();
#else
    return -1;
#endif
}

qint64 QnPerformance::currentCpuUsage() {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return refresherInstance()->usage();
#else
    return -1;
#endif
}

qint64 QnPerformance::currentCpuTotalUsage() {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return refresherInstance()->totalUsage();
#else
    return -1;
#endif
}

QString QnPerformance::cpuBrand() {
    return *qn_estimatedCpuBrand();
}

int QnPerformance::cpuCoreCount() {
    return QThread::idealThreadCount();
}

bool QnPerformance::currentHddUsage(QList<int> *output){
    return refresherInstance()->hddUsage(output);
}

