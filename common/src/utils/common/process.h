/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_PROCESS_H
#define NX_PROCESS_H

#include "systemerror.h"


//!Kills process by calling TerminateProcess on mswin and sending SIGKILL on unix
SystemError::ErrorCode killProcessByPid( qint64 pid );

#endif  //NX_PROCESS_H
