#include "performance.h"
#include <QtGlobal>

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
    class WmiRefresher{

    public:
        WmiRefresher(){
            m_locator = NULL;
            m_service = NULL;
            m_cpu_count = -1;
            m_prev_cpu = 0;
            m_prev_ts = 0;
            m_usage = 50;
            m_initialized = init();
        }

        ~WmiRefresher(){
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
            m_timer = SetTimer(0, 0, 2000, timerCallback);
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

    Q_GLOBAL_STATIC(WmiRefresher, refresherInstance);
    // initializer for Q_GLOBAL_STATIC singletone
    WmiRefresher* dummy = refresherInstance();

    VOID CALLBACK WmiRefresher::timerCallback(HWND /*hwnd*/, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
        refresherInstance()->refresh();
    }

} // anonymous namespace
#endif

#ifndef Q_OS_WIN
namespace{
    char* GetSystemOutput(char* cmd)
    {
        int buff_size = 32;
        char* buff = new char[buff_size];

        char* ret = NULL;
        string str = "";

        int fd[2];
        int old_fd[3];
        pipe(fd);

        old_fd[0] = dup(STDIN_FILENO);
        old_fd[1] = dup(STDOUT_FILENO);
        old_fd[2] = dup(STDERR_FILENO);

        int pid = fork();
        switch(pid)
        {
        case 0:
            close(fd[0]);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            dup2(fd[1], STDOUT_FILENO);
            dup2(fd[1], STDERR_FILENO);
            system(cmd);
            //execlp((const char*)cmd, cmd,0);
            close (fd[1]);
            exit(0);
            break;
        case -1:
            cerr << "GetSystemOutput/fork() error\n" << endl;
            exit(1);
        default:
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);

            int rc = 1;
            while (rc > 0)
            {
                rc = read(fd[0], buff, buff_size);
                str.append(buff, rc);
                //memset(buff, 0, buff_size);
            }

            ret = new char [strlen((char*)str.c_str())];

            strcpy(ret, (char*)str.c_str());

            waitpid(pid, NULL, 0);
            close(fd[0]);
        }

        dup2(STDIN_FILENO, old_fd[0]);
        dup2(STDOUT_FILENO, old_fd[1]);
        dup2(STDERR_FILENO, old_fd[2]);

        return ret;
    }
} //anonymous namespace
#endif

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
    // const 14 - length of substring 'model name : '
    return GetSystemOutput("grep 'model name' /proc/cpuinfo | head -1 | awk '{print substr($0, 14)}'");
#endif
    }
    Q_GLOBAL_STATIC_WITH_INITIALIZER(QString, qn_estimatedCpuBrand, { *x = estimateCpuBrand(); });
} // anonymous namespace

namespace {
    int estimateCpuCores() {
        int qt_count = QThread::idealThreadCount();
        if (qt_count > 0)
            return qt_count;

#ifdef Q_OS_WIN
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
#else
        // does not support multi-core processors yes
        // 'cpu cores' check is required
        return QString(GetSystemOutput("grep 'processor' /proc/cpuinfo | wc -l")).toInt();
#endif
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
#ifdef Q_OS_WIN
    return refresherInstance()->usage();
#else
    return 50;
#endif
}

QString QnPerformance::getCpuBrand(){
    return *qn_estimatedCpuBrand();
}

int QnPerformance::getCpuCores(){
    return *qn_estimatedCpuCores();
#endif
}

