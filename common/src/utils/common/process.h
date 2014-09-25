/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_PROCESS_H
#define NX_PROCESS_H

#include "systemerror.h"


namespace nx
{
    //!Kills process by calling TerminateProcess on mswin and sending SIGKILL on unix
    SystemError::ErrorCode killProcessByPid( qint64 pid );

    //!Start process and detach from it
    /*!
        On linux this function uses \a posix_spawn, since \a fork is too heavy (especially on ARM11 platform)
        \param executablePath Path to executable binary or shell script
        \param arguments
        \param workingDirectory If not empty, working directory is set to this path
        \param pid if not \a nullptr, launched process pid is stored here
        \return Error code
    */
    SystemError::ErrorCode startProcessDetached(
        const QString& executablePath,
        const QStringList& arguments,
        const QString& workingDirectory = QString(),
        qint64* pid = nullptr );
}

#endif  //NX_PROCESS_H
