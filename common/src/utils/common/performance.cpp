#include "performance.h"

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


/** CPU usage update timer, in seconds. */
#define CPU_USAGE_REFRESH 1

// -------------------------------------------------------------------------- //
// CPU Usage
// -------------------------------------------------------------------------- //
#if defined(Q_OS_WIN)

namespace {

    class CpuUsageRefresher {
    public:
        CpuUsageRefresher() {
            m_locator = NULL;
            m_service = NULL;
            m_cpu = 0;
            m_ts = 0;
            m_usage = 0;
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

    private:
        static VOID CALLBACK timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/);

        void refresh() {
            char q_str[100];
            sprintf(q_str, "SELECT * FROM Win32_PerfRawData_PerfProc_Thread WHERE IdProcess=%d", QCoreApplication::applicationPid());
            IEnumWbemClassObject* pEnumerator = query(q_str);
            if (pEnumerator){
                IWbemClassObject *pclsObj;
                ULONG uReturn = 0;
                quint64 cpu = 0;
                quint64 ts = 0;

                pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                while (uReturn != 0)
                {
                    VARIANT vtProc;

                    pclsObj->Get(L"PercentProcessorTime", 0, &vtProc, 0, 0);
                    QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal)); // TODO: check VARIANT type (vtProc.vt), or maybe write a more generic variant-to-longlong conversion function.
                    cpu = cpu + qstr.toULongLong();
                    VariantClear(&vtProc);

                    if (ts == 0){
                        pclsObj->Get(L"Timestamp_Sys100NS",0, &vtProc, 0, 0);
                        QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal));
                        ts = qstr.toULongLong();
                    }
                    VariantClear(&vtProc);
                    pclsObj->Release();

                    pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                }
                pEnumerator->Release();

                if (m_ts > 0){
                    m_usage = ((cpu - m_cpu) * 100) / ((ts - m_ts) * QnPerformance::cpuCoreCount());
                }
                m_cpu = cpu;
                m_ts = ts;
            }
        }

        bool init() {
            if (QSysInfo::windowsVersion() <= QSysInfo::WV_2000) {
                qnWarning("Windows version too low to use WMI");
                return false;
            }

            HRESULT hres;

            // Step 1: --------------------------------------------------
            // Initialize COM. ------------------------------------------
            hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
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
            m_timer = SetTimer(0, 0, CPU_USAGE_REFRESH * 1000, timerCallback);
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
        /** Flag of successful initialization. */
        bool m_initialized;

        /** WMI namespace locator interface. */
        IWbemLocator *m_locator;

        /** WMI services provider interface. */ 
        IWbemServices *m_service;

        /** Previous value of raw WMI PercentProcessorTime stat. */
        quint64 m_cpu;

        /** Previous value of raw WMI Timestamp_Sys100NS stat. */
        quint64 m_ts;

        /** Processor usage percent for the last CPU_USAGE_REFRESH seconds. */
        uint m_usage;

        /** System timer for processor usage calculating. */
        UINT_PTR m_timer;
    };

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance);
    // initializer for Q_GLOBAL_STATIC singleton
    CpuUsageRefresher *dummy = refresherInstance();

    VOID CALLBACK CpuUsageRefresher::timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
        refresherInstance()->refresh();
    }

} // anonymous namespace

#elif defined(Q_OS_LINUX)

namespace{
// very useful function, I'd move it to any shared linux util class
    static QString getSystemOutput(QString cmds)
    {
        QStringList list = cmds.split('|');
        QStringListIterator iter(list);

        if (!iter.hasNext())
            return "";
        QString cmd = iter.next();
        QProcess *prev = new QProcess();

        while(iter.hasNext()){
            QProcess *next = new QProcess();
            prev->setStandardOutputProcess(next);
            prev->start(cmd);
            if (!prev->waitForStarted())
                return "";
            if (!prev->waitForFinished())
                return "";

            prev = next;
            cmd = iter.next();
        }
        prev->start(cmd);
        if (!prev->waitForStarted())
            return "";
        if (!prev->waitForFinished())
            return "";

        return QString(prev->readAll());
    }

    static void timerCallback(int sig, siginfo_t *si, void *uc);

    class CpuUsageRefresher {
    public:
        CpuUsageRefresher() {
            m_cpu = 0;
            m_process_cpu = 0;
            m_usage = 0;
            m_timer = 0;
            m_initialized = init();
        }

        ~CpuUsageRefresher() {
            if (m_timer)
                timer_delete(m_timer);
        }

        uint usage() {
            return m_usage;
        }

        void** getTimerId() {
            return &m_timer;
        }

        void refresh() {
            qulonglong cpu = getCpu();
            qulonglong process_cpu = getProcessCpu();
            if (process_cpu > 0 && m_cpu > 0){
                m_usage = ((process_cpu - m_process_cpu) * 100) / (cpu - m_cpu);
            }
            m_cpu = cpu;
            m_process_cpu = process_cpu;
        }

    private:
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
            its.it_value.tv_sec = CPU_USAGE_REFRESH;
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
        qulonglong getCpu() {
            qulonglong result = 0;

            QString proc_stat = getSystemOutput("cat /proc/stat | grep \"cpu \"").mid(5);
            QStringList list = proc_stat.split(' ', QString::SkipEmptyParts);
            QStringListIterator iter(list);
            while (iter.hasNext())
                result += iter.next().toULongLong();
            return result;
        }

        qulonglong getProcessCpu() {
            QString request = QString("cat /proc/%1/stat").arg(QCoreApplication::applicationPid ());
            QStringList proc_stat = getSystemOutput(request).split(' ', QString::SkipEmptyParts);
            if (proc_stat.count() > 14){
                return (proc_stat[13]).toULongLong() +
                        proc_stat[14].toULongLong();
            }
            else{
                qWarning("Linux: proc stat is absent");
                return 0;
            }
        }

        bool m_initialized;
        quint64 m_cpu;
        quint64 m_process_cpu;
        uint m_usage;
        timer_t m_timer;
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
    return brandString;
#elif defined(Q_OS_LINUX)
    // 13 - const length of 'model name : ' string with tabs - standard output of /proc/cpuinfo
    return getSystemOutput("grep \"model name\" /proc/cpuinfo | head -1").mid(13);
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

BOOL getQueryThreadCycleTimeDefault(HANDLE , PULONG64 ){
    return false;
}

class Performance{
    typedef BOOL (*PtrQueryThreadCycleTime)(HANDLE ThreadHandle, PULONG64 CycleTime);

public:
    Performance::Performance(){
#ifdef Q_OS_WIN
        QLibrary Kernel32Lib(QString::fromAscii("Kernel32"));    
        PtrQueryThreadCycleTime result = (PtrQueryThreadCycleTime) Kernel32Lib.resolve("QueryThreadCycleTime");
        if (result)
            queryThreadCycleTime = result;
        queryThreadCycleTime = getQueryThreadCycleTimeDefault;
#else
        queryThreadCycleTime = getQueryThreadCycleTimeDefault;
#endif
    }
    PtrQueryThreadCycleTime queryThreadCycleTime;
};

Q_GLOBAL_STATIC(Performance, performanceInstance);


qint64 QnPerformance::currentThreadCycles() {
    ULONG64 time;
    
    BOOL status = performanceInstance()->queryThreadCycleTime(GetCurrentThread(), &time);
    if(!SUCCEEDED(status))
        return -1;
    return time;
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

QString QnPerformance::cpuBrand() {
    return *qn_estimatedCpuBrand();
}

int QnPerformance::cpuCoreCount() {
    return QThread::idealThreadCount();
}

