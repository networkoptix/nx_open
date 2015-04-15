#include "systemexcept_linux.h"

#include <execinfo.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// It is basicly more safe to use POSIX threads instead of any kind of wrappers,
// just to be sure that no memory alocations are taking place in the signal handlers
#include <pthread.h>

#include <QDebug>

static const int BUFFER_SIZE = 2048;
static const int BT_SIZE = 100;
static const int THREAD_COUNT = 100;
static const int SIGNALS[] = { SIGQUIT, SIGILL, SIGFPE, SIGSEGV, SIGBUS };

static const struct timespec WAIT_FOR_MUTEX     = { 1 /* sec */, 0 /* nsec */ };
static const struct timespec WAIT_FOR_THREADS   = { 1 /* sec */, 0 /* nsec */ };
static const struct timespec WAIT_FOR_MAIN      = { 1 /* sec */, 500000 /* nsec */ };

/** Thread Keeper without heap usage */
class ThreadKeeper
{
public:
    ThreadKeeper() : m_threads{} {}

    bool add(pthread_t thread)
    {
        for (auto it = &m_threads[0]; it != &m_threads[THREAD_COUNT]; ++it)
            if (*it == 0)
                return (*it = thread);
        return true;
    }

    bool remove(pthread_t thread)
    {
        for (auto it = &m_threads[0]; it != &m_threads[THREAD_COUNT]; ++it)
            if (*it == thread)
                return !(*it = 0);
        return false;
    }

    pthread_t first() const
    {
        for (auto it = &m_threads[0]; it != &m_threads[THREAD_COUNT]; ++it)
            if (*it != 0)
                return *it;
        return 0;
    }

private:
    pthread_t m_threads[THREAD_COUNT];
};

static std::string crashFile;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mainThread(0);
static ThreadKeeper allThreads;
static struct sigaction originalHandlers[_NSIG] = {};

// printf for fd without heap usage for sure
template<class ... Args>
static int printFd(int fd, Args ... args)
{
     char buffer[BUFFER_SIZE];
     if (const int len = sprintf(buffer, args ...))
        return write(fd, buffer, len);
     return 0;
}

static void printThreadData(int fd, pthread_t thread, int signal,
                            siginfo_t* /*info*/, ucontext_t* /*context*/)
{
    void *bt[BT_SIZE];
    if (const auto size = backtrace(bt, BT_SIZE))
    {
        printFd(fd, "%s (%d) in thread 0x%x, backtrace %d frames:\n\n",
            strsignal(signal), signal, static_cast<int>(thread), size);

        // TODO: consider to save some extra information from info and context
        backtrace_symbols_fd(bt, size, fd);
        printFd(fd, "\n\n");
        syncfs(fd);
    }
}

static void signalHandler(int signal, siginfo_t* info, void* data)
{
    const auto thisThread = pthread_self();
    static const auto problemThread = thisThread;
    static const auto logFile = open(crashFile.c_str(), O_CREAT | O_WRONLY, 0666);
    printThreadData(logFile, thisThread, signal, info, static_cast<ucontext_t*>(data));

    // Here we're using the mutex, shared with the rest of the program. This is not safe
    // and may lead to a deadlock! To avoid this we just will not ask for more backtareces
    // and quit right here
    if (pthread_mutex_timedlock(&mutex, &WAIT_FOR_MUTEX))
    {
        printFd(logFile, "Posible deadlock has been detected, quit...");
        originalHandlers[signal].sa_sigaction(signal, info, data);
        return;
    }

    allThreads.remove(thisThread);

    // send signal the next thread to write it's backtarece
    const auto nextThread = allThreads.first();
    if (!nextThread) return;
    pthread_mutex_unlock(&mutex);
    pthread_kill(nextThread, SIGQUIT);

    // The problem thread should be the one who finally delivers the signal to the initial
    // signal handler (to generate core dump, etc)
    // Both problem and main threads must survive long enouth to let other threads write
    // their backtraces into file
    if (thisThread == problemThread)
    {

        nanosleep(&WAIT_FOR_THREADS, 0);
        originalHandlers[signal].sa_sigaction(signal, info, data);
    }
    else
    if (thisThread == mainThread)
    {
        nanosleep(&WAIT_FOR_MAIN, 0);
        abort(); // shell not happend, but just to be sure!
    }
}

void linux_exception::installCrashSignalHandler()
{
    mainThread = pthread_self();
    {
        std::ostringstream os;
        os << getpwuid(getuid())->pw_dir << "/"
           << program_invocation_name << "_" << getpid() << ".crash";
        crashFile = os.str();
    }

    struct sigaction action;
    action.sa_sigaction = signalHandler;
    for (auto sig = &SIGNALS[0]; sig != &SIGNALS[sizeof(SIGNALS)/sizeof(int)]; ++sig)
        if (sigaction(*sig, &action, &originalHandlers[*sig]))
        {
            qDebug() << "Could not setup handler for " << strsignal(*sig)
                     << " (" << *sig << ")";
            return;
        }

    // main is also a thread :)
    installQuitThreadBacktracer();
}

void linux_exception::installQuitThreadBacktracer()
{
    pthread_mutex_lock(&mutex);
    allThreads.add(pthread_self());
    pthread_mutex_unlock(&mutex);
}

void linux_exception::uninstallQuitThreadBacktracer()
{
    pthread_mutex_lock(&mutex);
    allThreads.remove(pthread_self());
    pthread_mutex_unlock(&mutex);
}

std::string linux_exception::getCrashDirectory()
{
    if (const auto pwd = getpwuid(getuid()))
        return pwd->pw_dir;
    return std::string();
}

std::string linux_exception::getCrashPattern()
{
    std::ostringstream os;
    os << program_invocation_name << "_*.*";
    return os.str();
}
