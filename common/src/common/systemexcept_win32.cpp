
#ifdef _WIN32

#include "systemexcept_win32.h"

#include <memory>
#include <fstream>
#include <sstream>
#include <string>

#include <Dbghelp.h>
#include <Windows.h>
#include <ShlObj.h>
#include <platform/win32_syscall_resolver.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>


class GlobalCrashDumpSettings
{
public:
    bool dumpFullMemory;

    GlobalCrashDumpSettings()
    :
        dumpFullMemory( false )
    {
    }
};

static GlobalCrashDumpSettings globalCrashDumpSettingsInstance;

typedef BOOL (*pfMiniDumpWriteDump) (
    HANDLE,
    DWORD,
    HANDLE,
    MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION,
    PMINIDUMP_USER_STREAM_INFORMATION ,
    PMINIDUMP_CALLBACK_INFORMATION );

static void LegacyDump( HANDLE );

static int GetBaseName( const char* name , DWORD len ) {
    for( DWORD i = len-1 ; i >= 0 ; --i ) {
        if( name[i] == '\\' || name[i] == '/' )
            return i+1;
    }
    return 0;
}

static bool GetProgramName( char* buffer ) {
    char sModuleName[1024];
    DWORD dwLen = ::GetModuleFileNameA(NULL,sModuleName,1024);
    if( dwLen == 1024 || dwLen < 0 )
        return false;
    int iBaseNamePos = GetBaseName( sModuleName , dwLen );
    CopyMemory( buffer , sModuleName + iBaseNamePos , dwLen - iBaseNamePos + 1 );
    return true;
}

static
void WriteDump( HANDLE hThread , PEXCEPTION_POINTERS ex ) {

    static const pfMiniDumpWriteDump MiniDumpWriteDumpAddress = 
        Win32FuncResolver::instance()->resolveFunction<pfMiniDumpWriteDump> (
        L"DbgHelp.dll",
        "MiniDumpWriteDump",
        NULL);
    if( MiniDumpWriteDumpAddress == NULL )
    {
        LegacyDump(hThread);
        return;
    }


    char sFileName[1024];
    char sProgramName[1024];
    char sAppData[MAX_PATH];

    if( !GetProgramName(sProgramName) )
        return;

    if( FAILED(SHGetFolderPathA(
            NULL,
            CSIDL_LOCAL_APPDATA,
            NULL,
            0,
            sAppData)))
        return;

    // it should not acquire any global lock internally. Otherwise
    // we may deadlock here. 
    int ret = sprintf(sFileName,"%s\\%s_%d.dmp", sAppData , sProgramName, ::GetCurrentProcessId());

    if( ret <0 )
        return;

    HANDLE hFile = ::CreateFileA(
        sFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if( hFile == INVALID_HANDLE_VALUE )
        return;

    MINIDUMP_EXCEPTION_INFORMATION sMDumpExcept; 

    sMDumpExcept.ThreadId           = ::GetCurrentThreadId(); 
    sMDumpExcept.ExceptionPointers  = ex; 
    sMDumpExcept.ClientPointers     = FALSE; 

    // This will generate the full minidump. I don't know the specific
    // requirements of our dump file. If it is used for online report
    // or online analyzing, we may need to generate small dump file 

    MINIDUMP_TYPE sMDumpType =  (MINIDUMP_TYPE)( MiniDumpWithFullMemoryInfo | 
                                                MiniDumpWithHandleData | 
                                                MiniDumpWithThreadInfo | 
                                            MiniDumpWithUnloadedModules ); 
    if( globalCrashDumpSettingsInstance.dumpFullMemory )
    {
        sMDumpType = (MINIDUMP_TYPE)( sMDumpType |
            MiniDumpWithFullMemory | MiniDumpWithCodeSegs | 
            MiniDumpWithDataSegs | MiniDumpWithProcessThreadData | MiniDumpWithFullAuxiliaryState);
    }

    MiniDumpWriteDumpAddress( 
        ::GetCurrentProcess(),
        ::GetCurrentProcessId(),
        hFile,
        sMDumpType,
        ex == NULL ? NULL : &sMDumpExcept,
        NULL,
        NULL );
    ::CloseHandle(hFile);
}

static void translate( unsigned int code , _EXCEPTION_POINTERS* ExceptionInfo ) {
    Q_UNUSED(code);
    WriteDump( GetCurrentThread() , ExceptionInfo );
    TerminateProcess( GetCurrentProcess(), 1 );
}

static LONG WINAPI unhandledSEHandler( __in struct _EXCEPTION_POINTERS* ExceptionInfo )
{
    translate(0,ExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD WINAPI dumpStackProc( LPVOID lpParam )
{
    //TODO/IMPL
    HANDLE targetThreadHandle = (HANDLE)lpParam;

    //suspending thread
    if( SuspendThread( targetThreadHandle ) == (DWORD)-1 )
    {
        //TODO/IMPL
    }

    //getting thread context
    CONTEXT threadContext;
    memset( &threadContext, 0, sizeof(threadContext) );
    threadContext.ContextFlags = (CONTEXT_FULL);
    if( !GetThreadContext( targetThreadHandle, &threadContext ) )
    {
        //TODO/IMPL
    }

    WriteDump(targetThreadHandle,NULL);
    //terminating process
    return TerminateProcess( GetCurrentProcess(), 1 ) ? 0 : 1;
}

static void dumpCrtError()
{
    //creating thread to dump call stack of current thread
    HANDLE currentThreadExtHandle = INVALID_HANDLE_VALUE;
    if( DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &currentThreadExtHandle, 0, FALSE, DUPLICATE_SAME_ACCESS ) &&
        CreateThread( NULL, 0, dumpStackProc, currentThreadExtHandle, 0, NULL ) != NULL )
    {
        ::Sleep( 10000 );
        return;
    }
    WriteDump(currentThreadExtHandle,NULL);
    TerminateProcess( GetCurrentProcess(), 1 );
}

static void invalidCrtCallParameterHandler(
   const wchar_t* /*expression*/,
   const wchar_t* /*function*/,
   const wchar_t* /*file*/,
   unsigned int /*line*/,
   uintptr_t /*pReserved*/ )
{
    dumpCrtError();
}

static void myPurecallHandler()
{
    dumpCrtError();
}

void win32_exception::installGlobalUnhandledExceptionHandler()
{
    //_set_se_translator( &win32_exception::translate );
    SetUnhandledExceptionFilter( &unhandledSEHandler );

    //installing CRT handlers (invalid parameter, purecall, etc.)
    _set_invalid_parameter_handler( invalidCrtCallParameterHandler );
    _set_purecall_handler( myPurecallHandler );
}

void win32_exception::installThreadSpecificUnhandledExceptionHandler()
{
    _set_se_translator( translate );
}

void win32_exception::setCreateFullCrashDump( bool isFull )
{
    globalCrashDumpSettingsInstance.dumpFullMemory = isFull;
}

#define MAX_SYMBOL_SIZE 1024

static
HANDLE 
CreateLegacyDumpFile() {
    char sProcessName[1024];
    char sFileName[1024];
    char sAppData[MAX_PATH];

    if( !GetProgramName(sProcessName) )
        return INVALID_HANDLE_VALUE;

    if( FAILED(SHGetFolderPathA(
        NULL,
        CSIDL_LOCAL_APPDATA,
        NULL,
        0,
        sAppData)))
        return INVALID_HANDLE_VALUE;

    int ret = sprintf(sFileName,"%s\\%s_%d.except", sAppData, sProcessName, ::GetCurrentProcessId());

    if( ret <=0 )
        return INVALID_HANDLE_VALUE;

    return ::CreateFileA(
        sFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
}

static
void FWriteFile( HANDLE hFile , const char* fmt , ... ) {
    va_list vl;
    va_start(vl,fmt);
    char pBuffer[1024];
    DWORD dwWritten;
    int iRet = vsprintf(pBuffer,fmt,vl);
    if( iRet <=0 )
        return;
    WriteFile(hFile,pBuffer,iRet,&dwWritten,NULL);
}

static
void LegacyDump( HANDLE hThread ) {
    STACKFRAME64 StackFrame;
    BOOL bRet;
    PIMAGEHLP_SYMBOL64 pImgSymbol;
    IMAGEHLP_MODULE64 ModuleName;
    DWORD64 dwDisp;
    CONTEXT ContextRecord;
    PCONTEXT pContextRecord = &ContextRecord;
    char pBuffer[MAX_SYMBOL_SIZE+sizeof(IMAGEHLP_SYMBOL64)];

    HANDLE hProcess = GetCurrentProcess();
    std::unique_ptr<void, decltype(&::CloseHandle)> hFile( CreateLegacyDumpFile(), ::CloseHandle );
    if( hFile.get() == INVALID_HANDLE_VALUE )
    {
        hFile.release();
        return;
    }

    memset( pContextRecord, 0, sizeof(CONTEXT) );
    pContextRecord->ContextFlags = (CONTEXT_FULL);
    if( !GetThreadContext( hThread, pContextRecord ) ) {
        pContextRecord = NULL;
    }


    if( pContextRecord ) {
#ifdef _WIN64
#else
        StackFrame.AddrPC.Offset = pContextRecord->Eip;
        StackFrame.AddrStack.Offset = pContextRecord->Esp;
        StackFrame.AddrFrame.Offset = pContextRecord->Ebp;
#endif
    }

    StackFrame.AddrPC.Mode = AddrModeFlat;
    StackFrame.AddrStack.Mode = AddrModeFlat;
    StackFrame.AddrFrame.Mode = AddrModeFlat;
    ModuleName.SizeOfStruct = sizeof(ModuleName);

    SymSetOptions( SYMOPT_UNDNAME );
    pImgSymbol = reinterpret_cast<PIMAGEHLP_SYMBOL64>(pBuffer);

    bRet = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    if( bRet == FALSE )
        return;

    FWriteFile(hFile.get(),"%s\n","-----------------Stack Trace-----------------");

    while( true ) {
        bRet = StackWalk64(
#ifndef _WIN64
            IMAGE_FILE_MACHINE_I386,
#else
            IMAGE_FILE_MACHINE_AMD64,
#endif
            GetCurrentProcess(),
            hThread,
            &StackFrame,
            pContextRecord,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL );

        if( bRet == FALSE ) {
            FWriteFile(hFile.get(),"%s","-----------------Done -------------------");
            return;
        }

        FWriteFile(hFile.get(),"%08x:",StackFrame.AddrPC.Offset);
        if( SymGetModuleInfo64(GetCurrentProcess(), StackFrame.AddrPC.Offset, &ModuleName) ) {
            FWriteFile(hFile.get()," %s",ModuleName.ModuleName);
        }
        pImgSymbol->Size = MAX_SYMBOL_SIZE+sizeof(IMAGEHLP_SYMBOL64);
        pImgSymbol->MaxNameLength = MAX_SYMBOL_SIZE;
        if( SymGetSymFromAddr64(
            hProcess,
            StackFrame.AddrPC.Offset,
            &dwDisp,
            pImgSymbol )) {
                FWriteFile(hFile.get()," %s() + 0x%x\n",pImgSymbol->Name, dwDisp);
        } else {
            FWriteFile(hFile.get(),"%s","\n");
        }
    }
}

#endif
