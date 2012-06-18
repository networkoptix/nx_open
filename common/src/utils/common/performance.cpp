#include "performance.h"
#include <QtGlobal>

// timer step in seconds
#define CPU_USAGE_REFRESH 2

#ifdef Q_OS_WIN
#   define NOMINMAX
#   include <Windows.h>
#endif

// cpu_usage block
#ifdef Q_OS_WIN
#include "warnings.h"

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")

namespace{
    // Does not work in Windows 2000 or earlier
    class CpuUsageRefresher{

    public:
        CpuUsageRefresher(){
            m_locator = NULL;
            m_service = NULL;
            m_cpu_count = -1;
            m_prev_cpu = 0;
            m_prev_ts = 0;
            m_usage = 50;
            m_initialized = init();
        }

        ~CpuUsageRefresher(){
            if(m_initialized){
                m_service->Release();
                m_locator->Release();     
                CoUninitialize();
            }
            if(m_timer != 0) {
                KillTimer(NULL, m_timer); 
                m_timer = 0;
            }
        }

        uint usage(){
            return m_usage;
        }

    private:
        static VOID CALLBACK timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/);

        void refresh(){
            char q_str[100];
            sprintf(q_str, "SELECT * FROM Win32_PerfRawData_PerfProc_Thread WHERE IdProcess=%d", GetCurrentProcessId());
            IEnumWbemClassObject* pEnumerator = query(q_str);
            if (pEnumerator){
                IWbemClassObject *pclsObj;
                ULONG uReturn = 0;
                qulonglong cpu = 0;
                qulonglong ts = 0;
                while (pEnumerator)
                {
                    pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

                    if(0 == uReturn)
                        break;

                    VARIANT vtProc;

                    pclsObj->Get(L"PercentProcessorTime", 0, &vtProc, 0, 0);
                    QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal));
                    cpu = cpu + qstr.toULongLong();

                    if (ts == 0){
                        pclsObj->Get(L"Timestamp_Sys100NS",0, &vtProc, 0, 0);
                        QString qstr((QChar*)vtProc.bstrVal, ::SysStringLen(vtProc.bstrVal));
                        ts = qstr.toULongLong();
                    }

                    VariantClear(&vtProc);
                    pclsObj->Release();
                }
                pEnumerator->Release();

                if (m_prev_ts > 0){
                    m_usage = ((cpu - m_prev_cpu) * 100) / ((ts - m_prev_ts) * m_cpu_count);
                    qnDebug("usage %1", m_usage);
                }
                m_prev_cpu = cpu;
                m_prev_ts = ts;
            }
        }

        bool init(){
            HRESULT hres;

            // Step 1: --------------------------------------------------
            // Initialize COM. ------------------------------------------
            hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
            if (FAILED(hres))
            {
                qnDebug("Failed to initialize COM library. Error code = 0x%1", QString::number(hres, 16));
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
                qnDebug("Failed to initialize security. Error code = 0x%1", QString::number(hres, 16));
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
                qnDebug("Failed to create IWbemLocator object. Err code = 0x%1", QString::number(hres, 16));
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
                qnDebug("Could not connect. Error code = 0x%1", QString::number(hres, 16)); 
                m_locator->Release();
                CoUninitialize();
                return false;
            }

            qnDebug("Connected to ROOT\\CIMV2 WMI namespace\n");


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
                qnDebug("Could not set proxy blanket. Error code = 0x%1", QString::number(hres, 16));  
                m_service->Release();
                m_locator->Release();     
                CoUninitialize();
                return false;
            }

            m_cpu_count = QnPerformance::getCpuCores();
            qnDebug("CpuInfo %1, cores %2\n", QnPerformance::getCpuBrand(), m_cpu_count);
            m_timer = SetTimer(0, 0, CPU_USAGE_REFRESH * 1000, timerCallback);
            return true;
        }

        // possibly will be used in other WMI requests
        IEnumWbemClassObject* query(char* query_str){
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
                qnDebug("Query failed. Error code = 0x%1", QString::number(hres, 16));
                return NULL;
            }
            return enumerator;
        }

    private:
        bool m_initialized;
        IWbemLocator *m_locator;
        IWbemServices *m_service;
        int m_cpu_count;
        quint64 m_prev_cpu;
        quint64 m_prev_ts;
        uint m_usage;
        UINT_PTR m_timer;


    };

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance);
    // initializer for Q_GLOBAL_STATIC singletone
    CpuUsageRefresher* dummy = refresherInstance();

    VOID CALLBACK CpuUsageRefresher::timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
        refresherInstance()->refresh();
    }

} // anonymous namespace
#else
#include <time.h>
#include <signal.h>
#include <errno.h>

#define SIG_CODE SIGRTMIN

namespace {
    class CpuUsageRefresher{

    public:
        CpuUsageRefresher(){
            m_cpu_count = -1;
            m_prev_cpu = 0;
            m_prev_ts = 0;
            m_usage = 50;
            m_initialized = init();
        }

        ~CpuUsageRefresher(){
            timer_delete(m_timer);
        }

        uint usage(){
            return m_usage;
        }

    private:
        static void timerCallback(int sig, siginfo_t *si, void *uc);

        void refresh(){
            qnDebug("callback");
        }

        bool init(){
            m_cpu_count = QnPerformance::getCpuCores();
            qnDebug("CpuInfo %1, cores %2\n", QnPerformance::getCpuBrand(), m_cpu_count);

            struct sigevent sev;
            struct itimerspec its;
            struct sigaction sa;

            /* Establish handler for timer signal */

            qnDebug("Establishing handler for signal %1\n", SIG_CODE);
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = timerCallback;
            sigemptyset(&sa.sa_mask);
            if (sigaction(SIG_CODE, &sa, NULL) == -1){
                qnDebug("Err %1", errno);
                return false;
            }


            /* Create the timer */
            qnDebug("Creating timer...\n");
            sev.sigev_notify = SIGEV_SIGNAL;
            sev.sigev_signo = SIG_CODE;
            sev.sigev_value.sival_ptr = &m_timer;
            if (timer_create(CLOCK_REALTIME, &sev, &m_timer) == -1){
                qnDebug("Err %1", errno);
                return false;
            }

            /* Start the timer */
            qnDebug("Starting timer...");
            its.it_value.tv_sec = CPU_USAGE_REFRESH;
            its.it_value.tv_nsec = 0;
            its.it_interval.tv_sec = its.it_value.tv_sec;
            its.it_interval.tv_nsec = its.it_value.tv_nsec;

            if (timer_settime(m_timer, 0, &its, NULL) == -1){
                qnDebug("Err %1", errno);
                return false;
            }
            return true;
        }

    private:
        bool m_initialized;
        int m_cpu_count;
        quint64 m_prev_cpu;
        quint64 m_prev_ts;
        uint m_usage;
        timer_t m_timer;
    };

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance);
    // initializer for Q_GLOBAL_STATIC singletone
    CpuUsageRefresher* dummy = refresherInstance();

    VOID CALLBACK CpuUsageRefresher::timerCallback(int sig, siginfo_t *si, void *uc){
        CpuUsageRefresher refresher = refresherInstance();
        if (si->si_value.sival_ptr == &(refresher->m_timer)){
            refresher->refresh();
        }
    }
}


#endif

namespace{
#ifndef Q_OS_WIN
    QString GetSystemOutput(char* cmd)
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
#endif
} //anonymous namespace

namespace {
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
} // anonymous namespace

namespace {
    QString estimateCpuBrand() {
#ifdef Q_OS_WIN
        int CPUInfo[4] = {-1};
        unsigned   nExIds, i =  0;
        char CPUBrandString[0x40];
        // Get the information associated with each extended ID.
        __cpuid(CPUInfo, 0x80000000);
        nExIds = CPUInfo[0];
        for (i=0x80000000; i<=nExIds; ++i)
        {
            __cpuid(CPUInfo, i);
            // Interpret CPU brand string
            if  (i == 0x80000002)
                memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
            else if  (i == 0x80000003)
                memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
            else if  (i == 0x80000004)
                memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        }
        return CPUBrandString;
#else
        // const 14 - length of substring 'model name : ' with tabs
        return GetSystemOutput("grep \"model name\" /proc/cpuinfo | head -1 | awk \"{print substr($0, 14)}\"");
#endif
    }
    Q_GLOBAL_STATIC_WITH_INITIALIZER(QString, qn_estimatedCpuBrand, { *x = estimateCpuBrand(); });
} // anonymous namespace

namespace {
    int estimateCpuCores() {
        return QThread::idealThreadCount();
    }

    Q_GLOBAL_STATIC_WITH_INITIALIZER(int, qn_estimatedCpuCores, { *x = estimateCpuCores(); });
} // anonymous namespace


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

qint64 QnPerformance::currentThreadCycles() {
#ifdef Q_OS_WIN
    ULONG64 time;
    BOOL status = QueryThreadCycleTime(GetCurrentThread(), &time);
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

qint64 QnPerformance::currentCpuUsage(){
    return refresherInstance()->usage();
#endif
}

QString QnPerformance::getCpuBrand(){
    return *qn_estimatedCpuBrand();
}

int QnPerformance::getCpuCores(){
    return *qn_estimatedCpuCores();
}

