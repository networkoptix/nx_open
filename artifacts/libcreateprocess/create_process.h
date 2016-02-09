
#ifndef NX_CREATE_PROCESS_H
#define NX_CREATE_PROCESS_H


/*!
    \return on success, pid of child process. On failure, -1
*/
int nx_startProcessDetached( const char* exeFilePath, char* arguments[] );

#endif  //NX_CREATE_PROCESS_H
