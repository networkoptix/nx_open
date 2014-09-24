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

#include <QtCore/QProcess>

#ifdef LIBCREATEPROCESS
#include <libcp/create_process.h>
#endif


namespace nx
{
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

    SystemError::ErrorCode startProcessDetached(
        const QString& executablePath,
        const QStringList& arguments,
        const QString& workingDirectory,
        qint64* pid )
    {
        //TODO #ak on linux should always use posix_spawn

    #ifdef LIBCREATEPROCESS
        //TODO #ak support workingDirectory
        //TODO #ak check if executablePath is script or binary

        qint64 childPid = 0;
        char** argv = new char*[arguments.size()+1+1+1];  //one for "bin/bash", one for script name, one for null
        const QString& executableAbsolutePath = updateDir.absoluteFilePath(executablePath);
        int argIndex = 0;
        argv[argIndex] = new char[sizeof("bin/bash")+1];
        strcpy( argv[argIndex], "bin/bash" );
        ++argIndex;
        argv[argIndex] = new char[executableAbsolutePath.size()+1];
        strcpy( argv[argIndex], executableAbsolutePath.toLatin1().constData() );
        ++argIndex;
        for( const QString& arg: arguments )
        {
            argv[argIndex] = new char[arg.size()+1];
            strcpy( argv[argIndex], arg.toLatin1().constData() );
            ++argIndex;
        }
        argv[argIndex] = NULL;

        childPid = nx_startProcessDetached( "/bin/bash", argv );
        if( childPid == -1 )
            return SystemError::getLastOSErrorCode();
        if( pid )
            *pid = childPid;
        return SystemError::noError;
    #else
        return QProcess::startDetached(executablePath, arguments, workingDirectory, pid)
            ? SystemError::noError
            : SystemError::fileNotFound;    //TODO #ak get proper error code
    #endif
    }
}
