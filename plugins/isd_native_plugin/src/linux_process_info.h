/**********************************************************
* 17 jun 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef LINUX_PROCESS_INFO_H
#define LINUX_PROCESS_INFO_H


namespace ProcessState
{
    enum Value
    {
        running,
        sleeping,
        diskSleep,
        stopped,
        tracingStop,
        zombie,
        dead,
        unknown
    };

    /*!
        \param strSize If -1, \a str MUST be NULL-terminated
    */
    Value fromString( const char* str, ssize_t strSize = -1 );
    const char* toString( Value val );
}

struct ProcessStatus
{
    //* Name: Command run by this process.

    //!Current state of the process
    ProcessState::Value state;

    //* Tgid: Thread group ID (i.e., Process ID).

    //* Pid: Thread ID (see gettid(2)).

    //* PPid: PID of parent process.

    //!PID of process tracing this process (0 if not being traced)
    pid_t tracerPid;

    //* Uid, Gid: Real, effective, saved set, and filesystem UIDs
    //(GIDs).

    //!Number of file descriptor slots currently allocated
    size_t FDSize;

    //* Groups: Supplementary group list.

    //!Peak virtual memory size
    size_t vmPeak;

    //!Virtual memory size
    size_t vmSize;

    //!Locked memory size (see mlock(3))
    size_t vmLck;

    //!Peak resident set size ("high water mark")
    size_t vmHWM;

    //!Resident set size
    size_t vmRSS;

    //!Size of data, stack, and text segments
    size_t vmData;
    //!Size of data, stack, and text segments
    size_t vmStk;
    //!Size of data, stack, and text segments
    size_t vmExe;

    //!Shared library code size
    size_t vmLib;

    //!Page table entries size (since Linux 2.6.10)
    size_t vmPTE;

    //!Number of threads in process containing this thread
    size_t threads;

    //* SigQ: This field contains two slash-separated numbers that
    //relate to queued signals for the real user ID of this
    //process.  The first of these is the number of currently
    //queued signals for this real user ID, and the second is the
    //resource limit on the number of queued signals for this
    //process (see the description of RLIMIT_SIGPENDING in
    //getrlimit(2)).

    //* SigPnd, ShdPnd: Number of signals pending for thread and for
    //process as a whole (see pthreads(7) and signal(7)).

    //* SigBlk, SigIgn, SigCgt: Masks indicating signals being
    //blocked, ignored, and caught (see signal(7)).

    //* CapInh, CapPrm, CapEff: Masks of capabilities enabled in
    //inheritable, permitted, and effective sets (see
    //capabilities(7)).

    //* CapBnd: Capability Bounding set (since kernel 2.6.26, see
    //capabilities(7)).

    //* Cpus_allowed: Mask of CPUs on which this process may run
    //(since Linux 2.6.24, see cpuset(7)).

    //* Cpus_allowed_list: Same as previous, but in "list format"
    //(since Linux 2.6.26, see cpuset(7)).

    //* Mems_allowed: Mask of memory nodes allowed to this process
    //(since Linux 2.6.24, see cpuset(7)).

    //* Mems_allowed_list: Same as previous, but in "list format"
    //(since Linux 2.6.26, see cpuset(7)).

    //* voluntary_context_switches, nonvoluntary_context_switches:
    //Number of voluntary and involuntary context switches (since
    //Linux 2.6.23).
};

bool readProcessStatus( ProcessStatus* const processStatus );

#endif  //LINUX_PROCESS_INFO_H
