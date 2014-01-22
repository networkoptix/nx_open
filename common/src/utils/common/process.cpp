/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "process.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#endif


SystemError::ErrorCode killProcessByPid( qint64 pid )
{
#ifdef _WIN32
    HANDLE hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, pid );
    if( hProcess == NULL )
        return SystemError::getLastOSErrorCode();
    const SystemError::ErrorCode result = TerminateProcess( hProcess, 1 )
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
    CloseHandle( hProcess );
    return result;
#else   //TODO: #ak check if it works on mac
    return kill( pid, SIGKILL ) == 0
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
#endif
}
