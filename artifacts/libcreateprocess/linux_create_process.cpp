
#include "create_process.h"

#include <iostream>
#include <stdlib.h>
#include <spawn.h>


/*!
    \return on success, pid of child process. On failure, -1
*/
int nx_startProcessDetached( const char* exeFilePath, char* argv[] )
{
    std::cout<<"Launching "<<exeFilePath<<std::endl;
    for( char** arg = argv; *arg; ++arg )
    {
        std::cout<<"    "<<*arg<<std::endl;
    }

    pid_t childPid = -1;
    if( posix_spawn(
            &childPid,
            exeFilePath,
            NULL,
            NULL,
            argv,
            NULL ) == 0 )
        return childPid;
    else
        return -1;

}
