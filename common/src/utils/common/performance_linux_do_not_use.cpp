#include "qnperformance.h"
#include <QtGlobal>
#include <QString>
#include <QProcess>
#include <QThread>

// cpu_usage block
#include <time.h>
#include <signal.h>
#include <iostream>
#include <errno.h>


namespace{
#ifndef Q_OS_WIN
    QString GetSystemOutput(QString cmds)
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

        }

        uint usage(){
            return m_usage;
        }

    private:
        static void timerCallback(int sig, siginfo_t *si, void *uc);

        void refresh(){
            qDebug("callback");
        }

        bool init(){
            m_cpu_count = QnPerformance::getCpuCores();
            QString debug = QString("CpuInfo %1, cores %2\n")
                    .arg(QnPerformance::getCpuBrand())
                    .arg(m_cpu_count);
            //qDebug() << debug;
            std::cout << "starting debug" << std::endl;
            std::cout << qPrintable(QnPerformance::getCpuBrand()) << std::endl;
            struct sigevent         te;
            struct itimerspec       its;
            struct sigaction        sa;
            int                     sigNo = SIGRTMIN;
            int intervalMS = 2000;
            int expireMS = 20000;

            /* Set up signal handler. */
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = timerCallback;
            sigemptyset(&sa.sa_mask);

            /* Set and enable alarm */
            te.sigev_notify = SIGEV_THREAD;
            te.sigev_signo = sigNo;
            te.sigev_value.sival_ptr = &m_timer;
            int tc = timer_create(CLOCK_REALTIME, &te, &m_timer);

            its.it_interval.tv_sec = 0;
            its.it_interval.tv_nsec = intervalMS * 1000000;
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = expireMS * 1000000;
            int tst = timer_settime(m_timer, 0, &its, NULL);
            if (tst == -1)
                std::cout << errno << std::endl;

            std::cout << tc << " " << tst << std::endl;
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

    Q_GLOBAL_STATIC(CpuUsageRefresher, refresherInstance)
    // initializer for Q_GLOBAL_STATIC singletone
    CpuUsageRefresher* dummy = refresherInstance();

    void CpuUsageRefresher::timerCallback(int sig, siginfo_t *si, void *uc){
        refresherInstance()->refresh();
    }
}

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
    std::cout << qPrintable(GetSystemOutput("grep \"model name\" /proc/cpuinfo | head -1 | awk \"{print substr($0, 14)}\"")) << std::endl;
    //std::cout << qPrintable(GetSystemOutput("grep \"model name\" /proc/cpuinfo | head -1")) << std::endl;
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
        // not so important cause QThread::idealThreadCount() is working in most cases
        return GetSystemOutput("grep 'processor' /proc/cpuinfo | wc -l").toInt();
#endif
    }

    Q_GLOBAL_STATIC_WITH_INITIALIZER(int, qn_estimatedCpuCores, { *x = estimateCpuCores(); })
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
}

QString QnPerformance::getCpuBrand(){
    return *qn_estimatedCpuBrand();
}

int QnPerformance::getCpuCores(){
    return *qn_estimatedCpuCores();
}

