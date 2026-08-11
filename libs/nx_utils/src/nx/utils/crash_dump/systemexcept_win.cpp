// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systemexcept_win.h"

#include <memory>
#include <fstream>
#include <sstream>
#include <string>

#include <Windows.h>
#include <Dbghelp.h>
#include <ShlObj.h>
#include <signal.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

#include "../platform/win32_syscall_resolver.h"

#define MAX_SYMBOL_SIZE 1024

static const std::string fullVersionId = nx::utils::AppInfo::vmsFullVersion().toStdString();

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

typedef BOOL (WINAPI *pfMiniDumpWriteDump) (
    HANDLE,
    DWORD,
    HANDLE,
    MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION,
    PMINIDUMP_USER_STREAM_INFORMATION ,
    PMINIDUMP_CALLBACK_INFORMATION );

static void LegacyDump( HANDLE );

static int GetBaseName(const char* name, DWORD len)
{
    NX_ASSERT(len > 0);
    if (len == 0)
        return 0;

    do
    {
        --len;
        if (name[len] == '\\' || name[len] == '/')
            return len + 1;
    }
    while (len > 0);
    return 0;
}

static bool GetProgramName( char* buffer ) {
    char sModuleName[MAX_SYMBOL_SIZE];

    // If the function fails, the return value is 0.
    DWORD dwLen = ::GetModuleFileNameA( NULL, sModuleName, MAX_SYMBOL_SIZE );
    if (dwLen == MAX_SYMBOL_SIZE || dwLen == 0)
        return false;

    int iBaseNamePos = GetBaseName( sModuleName , dwLen );
    CopyMemory( buffer , sModuleName + iBaseNamePos , dwLen - iBaseNamePos + 1 );

    // replace spaces with underscores
    for ( auto it = buffer; *it; ++it )
        if ( *it == ' ' )
            *it = '_';

    return true;
}

static bool GetCrashPrefix( char* sCrashPrefix )
{
    char sProgramName[MAX_SYMBOL_SIZE];
    if( !GetProgramName( sProgramName ) )
        return false;

    return sprintf( sCrashPrefix, "%s_%s", sProgramName, fullVersionId.c_str());
}

// Customer-defined exception codes (bit 29 set), used for CRT failures that are reported to us
// through a callback rather than as an SEH exception, and thus have no exception code of their
// own. Giving the dump a synthetic-but-real exception record is what makes it self-triaging:
// without one the dump has no exception stream at all, debuggers report a placeholder breakpoint
// at address 0, and !analyze blames thread 0 instead of the thread that actually failed.
static const DWORD kPureVirtualCallExceptionCode = 0xE0000001;
static const DWORD kInvalidCrtParameterExceptionCode = 0xE0000002;

// NOTE: x86-64 only, as is the whole build. On any other architecture CONTEXT has no Rip member
// (32-bit x86 has Eip, ARM64 has Pc), so this fails to compile rather than silently misreporting.
static EXCEPTION_RECORD MakeCrtExceptionRecord(DWORD exceptionCode, const CONTEXT& context)
{
    return {
        .ExceptionCode = exceptionCode,
        .ExceptionFlags = EXCEPTION_NONCONTINUABLE,
        .ExceptionAddress = reinterpret_cast<PVOID>(context.Rip),
    };
}

struct DumpThreadParams
{
    HANDLE targetThreadHandle = nullptr;
    DWORD targetThreadId = 0;
    DWORD exceptionCode = 0;

    // Captured on the failing thread at CRT handler entry, so it names the fault site and not
    // wherever that thread has moved to by the time the dump is written.
    CONTEXT context{.ContextFlags = CONTEXT_FULL};
};

// Static rather than a stack local of the failing thread: dumpStackProc() outlives its caller's
// frame whenever SuspendThread() fails, and writing a full-memory dump can take far longer than
// the caller's sleep. Only one crash is ever dumped, so a single instance is enough.
static DumpThreadParams globalDumpThreadParams;

static void WriteDump(HANDLE hThread, PEXCEPTION_POINTERS ex, DWORD faultingThreadId)
{
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

    char sAppData[MAX_PATH];
    if( FAILED(SHGetFolderPathA(
            NULL,
            CSIDL_LOCAL_APPDATA,
            NULL,
            0,
            sAppData)))
        return;

    char sCrashPrefix[MAX_PATH];
    if( !GetCrashPrefix( sCrashPrefix ) )
        return;

    // it should not acquire any global lock internally. Otherwise
    // we may deadlock here.
    char sFileName[MAX_PATH];
    if( sprintf(sFileName, "%s\\%s_%i.dmp", sAppData, sCrashPrefix, GetCurrentProcessId() ) < 0)
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

    // NOTE: Must be the thread that actually failed, which is not necessarily the caller - the
    // CRT-failure path writes the dump from a helper thread.
    sMDumpExcept.ThreadId = faultingThreadId;
    sMDumpExcept.ExceptionPointers = ex;
    sMDumpExcept.ClientPointers = FALSE;

    // This will generate the full minidump. I don't know the specific
    // requirements of our dump file. If it is used for online report
    // or online analyzing, we may need to generate small dump file

    MINIDUMP_TYPE sMDumpType =  (MINIDUMP_TYPE)( MiniDumpWithFullMemoryInfo |
                                                MiniDumpWithHandleData |
                                                MiniDumpWithThreadInfo |
                                            MiniDumpWithUnloadedModules );
    if( globalCrashDumpSettingsInstance.dumpFullMemory )
    {
        //TODO #akolesnikov if add MiniDumpWithCodeSegs, generated dump has 0 size
        sMDumpType = (MINIDUMP_TYPE)( sMDumpType |
            MiniDumpWithFullMemory |
            MiniDumpWithDataSegs | MiniDumpWithProcessThreadData | MiniDumpWithFullAuxiliaryState );
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
    WriteDump(GetCurrentThread(), ExceptionInfo, ::GetCurrentThreadId());
    TerminateProcess( GetCurrentProcess(), 1 );
}

static LONG WINAPI unhandledSEHandler( __in struct _EXCEPTION_POINTERS* ExceptionInfo )
{
    translate(0,ExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD WINAPI dumpStackProc(LPVOID /*lpParam*/)
{
    DumpThreadParams& params = globalDumpThreadParams;

    // Suspend the failing thread purely to freeze its memory/registers while the dump is being
    // written. Its CONTEXT was already captured on that thread itself, at CRT handler entry -
    // there is no need (and, since the thread has since moved on into this file's own Sleep()
    // call, no correctness) to fetch it again here via GetThreadContext().
    SuspendThread(params.targetThreadHandle);

    EXCEPTION_RECORD exceptionRecord =
        MakeCrtExceptionRecord(params.exceptionCode, params.context);
    EXCEPTION_POINTERS exceptionPointers{
        .ExceptionRecord = &exceptionRecord,
        .ContextRecord = &params.context};

    WriteDump(params.targetThreadHandle, &exceptionPointers, params.targetThreadId);

    //terminating process
    return TerminateProcess( GetCurrentProcess(), 1 ) ? 0 : 1;
}

static void dumpCrtError(DWORD exceptionCode, const CONTEXT& context)
{
    globalDumpThreadParams.exceptionCode = exceptionCode;
    globalDumpThreadParams.context = context;

    // Dump the call stack of this thread from a helper thread, so that this one can be suspended
    // and its registers/memory frozen while the dump is written.
    HANDLE currentThreadExtHandle = INVALID_HANDLE_VALUE;
    if (DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &currentThreadExtHandle,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS))
    {
        globalDumpThreadParams.targetThreadHandle = currentThreadExtHandle;
        globalDumpThreadParams.targetThreadId = ::GetCurrentThreadId();

        if (CreateThread(nullptr, 0, dumpStackProc, nullptr, 0, nullptr))
        {
            // dumpStackProc() suspends this thread and then terminates the process, so this sleep
            // normally never elapses. It only matters if SuspendThread() failed, and it has to
            // outlast writing a full-memory dump, which can take tens of seconds.
            ::Sleep(120'000);
            return;
        }

        ::CloseHandle(currentThreadExtHandle);
    }

    // Fallback: the helper thread could not be started, so dump from this thread directly, using
    // the context already captured by the caller.
    EXCEPTION_RECORD exceptionRecord = MakeCrtExceptionRecord(exceptionCode, context);
    EXCEPTION_POINTERS exceptionPointers{
        .ExceptionRecord = &exceptionRecord,
        .ContextRecord = const_cast<CONTEXT*>(&context)};

    WriteDump(GetCurrentThread(), &exceptionPointers, ::GetCurrentThreadId());
    TerminateProcess(GetCurrentProcess(), 1);
}

// A macro, not a helper: RtlCaptureContext() records the frame of *its* caller, and that PC
// becomes the dump's ExceptionAddress and hence !analyze's FAILURE_BUCKET_ID. Expanding at the
// handler keeps that symbol stable; capturing in a helper - or in dumpCrtError(), static with two
// call sites - would leave it to the inliner.
#define NX_CAPTURE_CRASH_CONTEXT(name) \
    CONTEXT name{.ContextFlags = CONTEXT_FULL}; \
    ::RtlCaptureContext(&name)

static void invalidCrtCallParameterHandler(
   const wchar_t* /*expression*/,
   const wchar_t* /*function*/,
   const wchar_t* /*file*/,
   unsigned int /*line*/,
   uintptr_t /*pReserved*/ )
{
    NX_CAPTURE_CRASH_CONTEXT(context);
    dumpCrtError(kInvalidCrtParameterExceptionCode, context);
}

static void pureVirtualCallHandler()
{
    NX_CAPTURE_CRASH_CONTEXT(context);
    dumpCrtError(kPureVirtualCallExceptionCode, context);
}

static void abortHandler(int signal)
{
    if (signal == SIGABRT)
    {
        // Cause access violation so abort is handled the same way, via the SEH path.
        // NOTE: dumpCrtError() would now also produce a dump with a usable exception record, but
        // routing through a genuine SEH exception keeps the real fault context, so it is kept.
        *static_cast<volatile int*>(0) = 7;
    }
}

void win32_exception::installGlobalUnhandledExceptionHandler()
{
    //_set_se_translator(&win32_exception::translate);
    SetUnhandledExceptionFilter(&unhandledSEHandler);

    // Install CRT handlers (invalid parameter, pure virtual call, etc.).
    _set_invalid_parameter_handler(invalidCrtCallParameterHandler);
    _set_purecall_handler(pureVirtualCallHandler);

    // Unhandled C++ exceptions, abort(), etc.
    signal(SIGABRT, abortHandler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
}

void win32_exception::setCreateFullCrashDump( bool isFull )
{
    globalCrashDumpSettingsInstance.dumpFullMemory = isFull;
}

std::string win32_exception::getCrashDirectory()
{
    char sAppData[MAX_PATH];
    if( FAILED(SHGetFolderPathA(
            NULL,
            CSIDL_LOCAL_APPDATA,
            NULL,
            0,
            sAppData)))
        return std::string( "." );

    return std::string( sAppData );
}

std::string win32_exception::getCrashPattern()
{
    char sCrashPrefix[MAX_SYMBOL_SIZE];
    if( !GetCrashPrefix( sCrashPrefix ) )
        return std::string();

    std::ostringstream oss;
    oss << sCrashPrefix << "_*.*";
    return oss.str();
}

static
HANDLE
CreateLegacyDumpFile() {
    char sAppData[MAX_PATH];
    if( FAILED(SHGetFolderPathA(
            NULL,
            CSIDL_LOCAL_APPDATA,
            NULL,
            0,
            sAppData)))
        return INVALID_HANDLE_VALUE;

    char sCrashPrefix[MAX_PATH];
    if( !GetCrashPrefix( sCrashPrefix ) )
        return INVALID_HANDLE_VALUE;

    // it should not acquire any global lock internally. Otherwise
    // we may deadlock here.
    char sFileName[MAX_PATH];
    if( sprintf( sFileName, "%s\\%s_%i.except", sAppData, sCrashPrefix, GetCurrentProcessId() ) < 0)
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
    va_end(vl);
    if( iRet <=0 )
        return;
    WriteFile(hFile,pBuffer,iRet,&dwWritten,NULL);
}

static
void LegacyDump( HANDLE hThread ) {
    STACKFRAME64 StackFrame{};
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
        StackFrame.AddrPC.Offset = pContextRecord->Rip;
        StackFrame.AddrStack.Offset = pContextRecord->Rsp;
        StackFrame.AddrFrame.Offset = pContextRecord->Rbp;
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
            IMAGE_FILE_MACHINE_AMD64,
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

        FWriteFile(hFile.get(), "%016llx:", StackFrame.AddrPC.Offset);
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
                FWriteFile(hFile.get(), " %s() + 0x%llx\n", pImgSymbol->Name, dwDisp);
        } else {
            FWriteFile(hFile.get(),"%s","\n");
        }
    }
}
