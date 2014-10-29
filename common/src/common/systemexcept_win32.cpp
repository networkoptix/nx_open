
#ifdef _WIN32

#include "systemexcept_win32.h"

#include <fstream>
#include <sstream>
#include <string>

#include <Dbghelp.h>
#include <Windows.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>

using std::string;

//typedef struct STACK_FRAME
//{
//    STACK_FRAME* Ebp;   //address of the calling function frame
//    PBYTE Ret_Addr;      //return address
//    DWORD Param[0];      //parameter list - could be empty
//} STACK_FRAME;
//typedef STACK_FRAME* PSTACK_FRAME;

const char* win32_exception::what() const 
{ 
    return m_what.c_str(); 
}

win32_exception::Address win32_exception::where() const 
{ 
    return mWhere; 
}

unsigned win32_exception::code() const 
{ 
    return mCode; 
}

bool access_violation::isWrite() const 
{ 
    return mIsWrite; 
}

win32_exception::Address access_violation::badAddress() const 
{ 
    return mBadAddress; 
}

static
void writeMiniDump( PEXCEPTION_POINTERS ex ) {
    char strFileName[1024];
    char strModuleName[1024];

    // should not have any heap allocation
    const char* strLocation = 
        QStandardPaths::writableLocation( QStandardPaths::DataLocation ).toLatin1().constData();

    // Get the current working directory
    DWORD dwLen = GetModuleFileNameA( NULL , strModuleName , 1024 );
    if( dwLen <=0 || dwLen == 1024 )
        return;

    // it should not acquire any global lock internally. Otherwise
    // we may deadlock here. 
    int ret = sprintf(strFileName,"%s/%s_%d.minidump",
        strLocation,
        strModuleName,
        GetCurrentProcessId());

    if( ret <0 )
        return;

    HANDLE hFile = ::CreateFileA(
        strFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if( hFile == INVALID_HANDLE_VALUE )
        return;

    MINIDUMP_EXCEPTION_INFORMATION sMDumpExcept; 

    sMDumpExcept.ThreadId           = GetCurrentThreadId(); 
    sMDumpExcept.ExceptionPointers  = ex; 
    sMDumpExcept.ClientPointers     = FALSE; 

    // This will generate the full minidump. I don't know the specific
    // requirements of our dump file. If it is used for online report
    // or online analyzing, we may need to generate small dump file 

    MINIDUMP_TYPE sMDumpType =  (MINIDUMP_TYPE)(MiniDumpWithFullMemory | 
                                            MiniDumpWithFullMemoryInfo | 
                                                MiniDumpWithHandleData | 
                                                MiniDumpWithThreadInfo | 
                                            MiniDumpWithUnloadedModules ); 

    ::MiniDumpWriteDump( 
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        sMDumpType,
        ex == NULL ? NULL : &sMDumpExcept,
        NULL,
        NULL);

    ::CloseHandle(hFile);
}


static LONG WINAPI unhandledSEHandler( __in struct _EXCEPTION_POINTERS* ExceptionInfo )
{
    win32_exception::translate( EXCEPTION_ACCESS_VIOLATION, ExceptionInfo );
    return EXCEPTION_EXECUTE_HANDLER;
}

static std::string getCallStack(
    HANDLE threadID,
    PEXCEPTION_RECORD exceptionRecord,
    PCONTEXT contextRecord );

static void writeCrashInfo(
    const char* title,
    const char* information )
{
#if 0
    // As bug 3355 indicates, the exception file should be saved to AppData folder on windows instead of
    // the client executable path for the permission problem. 
    const QFileInfo exeFileInfo( QCoreApplication::applicationFilePath() );
    const QString exceptFileName = lit("%1/%2_%3.except").
        arg( QStandardPaths::writableLocation( QStandardPaths::DataLocation ) ).
        arg( exeFileInfo.baseName() ).
        arg( QCoreApplication::applicationPid() );
    std::ofstream of( sFileName );
    of<<title<<"\n";
    of.write( information, strlen(information) );
    of.close();
#else
    // The following code will try my best to avoid (to my best knowledge)
    // 1) Heap allocation ( 1. Heap corruption 2. CRT global lock )
    // 2) Function that cannot be re-entered
    // 3) Function that acquires global lock ( most of CRT function )
    char sProcessName[1024];
    char sFileName[1024];
    DWORD dwLen = GetModuleFileNameA(NULL,sProcessName,1024);
    if( dwLen <0 )
        return;
    int ret = sprintf(sFileName,"%s/%s_%d.except",
        // Cannot avoid this one since it is determined by Qt Runtime 
        // But since Qt uses copy-on-write, there is no reason this will
        // have internal heap allocation .
        QStandardPaths::writableLocation(QStandardPaths::DataLocation).toLatin1().constData(),
        sProcessName,
        GetCurrentProcessId());
    if( ret <=0 )
        return;

    HANDLE hFile = ::CreateFileA(
        sFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if(hFile == INVALID_HANDLE_VALUE)
        return;

    // Output the title
    BOOL bRet;

    // This parameter is not in used for writing into file and the WriteFile ensure
    // that the write will not partially succeed, but we still need it to make SDK happy.
    DWORD dwWritten;

    bRet = ::WriteFile(hFile,title,(DWORD)(strlen(title)),&dwWritten,NULL);
    if( bRet == FALSE )
        goto done;

    bRet = ::WriteFile(hFile,"\n",1,&dwWritten,NULL);
    if( bRet == FALSE )
        goto done;

    bRet = ::WriteFile(hFile,information,(DWORD)(strlen(information)),&dwWritten,NULL);
    if( bRet == FALSE )
        goto done;

done:
    ::CloseHandle(hFile);
#endif 
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

    //dumping stack
    const std::string& currentCallStack = getCallStack( targetThreadHandle, NULL, &threadContext );
    writeCrashInfo( "CRT_INVALID_PARAMETER", currentCallStack.c_str() );
    writeMiniDump(NULL);

    //terminating process
    return TerminateProcess( GetCurrentProcess(), 1 ) ? 0 : 1;
}

static void dumpCrtError( const char* errorName )
{
    //creating thread to dump call stack of current thread
    HANDLE currentThreadExtHandle = INVALID_HANDLE_VALUE;
    if( DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &currentThreadExtHandle, 0, FALSE, DUPLICATE_SAME_ACCESS ) &&
        CreateThread( NULL, 0, dumpStackProc, currentThreadExtHandle, 0, NULL ) != NULL )
    {
        ::Sleep( 10000 );
        return;
    }

    const std::string& currentCallStack = getCallStack( GetCurrentThread(), NULL, NULL );
    //NOTE on win64 currentCallStack will be empty
    writeCrashInfo( errorName, currentCallStack.c_str() );
    writeMiniDump(NULL);

    TerminateProcess( GetCurrentProcess(), 1 );
}

static void invalidCrtCallParameterHandler(
   const wchar_t* /*expression*/,
   const wchar_t* /*function*/,
   const wchar_t* /*file*/,
   unsigned int /*line*/,
   uintptr_t /*pReserved*/ )
{
    dumpCrtError( "CRT_INVALID_PARAMETER" );
}

static void myPurecallHandler()
{
    dumpCrtError( "PURE_VIRTUAL_CALL" );
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
    _set_se_translator( &win32_exception::translate );
}

void win32_exception::translate(
    unsigned int code, 
    PEXCEPTION_POINTERS info )
{
    switch( code )
    {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            access_violation e( info );
            writeCrashInfo( "EXCEPTION_ACCESS_VIOLATION", e.what() );
            writeMiniDump(info);
            break;
        }
    
        default:
        {
            win32_exception e( info );
            writeCrashInfo( "STRUCTURED EXCEPTION", e.what() );
            writeMiniDump(info);
            break;
        }
    }

    TerminateProcess( GetCurrentProcess(), 1 );
}


#define SYMSIZE 10000
#define MAXTEXT 5000

static char *prregs( char *str, PCONTEXT cr )
{
    char *cp= str;
#ifndef _WIN64
    sprintf(cp, "\t%s:%08x", "Eax", cr->Eax); cp += strlen(cp);
    sprintf(cp, " %s:%08x",  "Ebx", cr->Ebx); cp += strlen(cp);
    sprintf(cp, " %s:%08x",  "Ecx", cr->Ecx); cp += strlen(cp);
    sprintf(cp, " %s:%08x\n","Edx", cr->Edx); cp += strlen(cp);
    sprintf(cp, "\t%s:%08x", "Esi", cr->Esi); cp += strlen(cp);
    sprintf(cp, " %s:%08x",  "Edi", cr->Edi); cp += strlen(cp);
    sprintf(cp, " %s:%08x",  "Esp", cr->Esp); cp += strlen(cp);
    sprintf(cp, " %s:%08x\n","Ebp", cr->Ebp); cp += strlen(cp);
#else
    //TODO/IMPL print x86_64 registers
#endif
    
    return str;
}

static int getword(
    FILE *fp, 
    char *word, 
    int n )
{
    int c;
    int k = 0;
    while( (c= getc(fp)) != EOF )
    {
        if(c<'0' || c>'z')
        {
            if( k>0 )
                break;
        }
        else if( --n<=0 )
        {
            break;
        }
        else
        {
            ++k;
            *word++= c;
        }
    }
    *word++= 0;
    return c!=EOF;
}

static int isnum( char *cp )
{
    int k;
    for( k = 0; cp[k]; ++k )
        if( !isxdigit(cp[k]) )
            return 0;
    return k==8;
}

static char *trmap(
    unsigned long addr, 
    const char *mapname, 
    DWORD64 *offset )
{
    char word[512];
    char lastword[512];
    static char wbest[512];
    unsigned long ubest= 0;
    FILE *fp = fopen( mapname, "rt" );
    if( !fp )
        return 0;
    while( getword(fp, word, sizeof(word)) )
    {
        if( isnum(word) )
        {
            char *endc;
            unsigned long u = strtoul( word, &endc, 16 );
            if( u<=addr && u>ubest )
            {
                strcpy( wbest, lastword );
                ubest= u;
            }
        }
        else
        {
            strcpy(lastword, word);
        }
    }
    fclose(fp);
    if( offset )
        *offset = addr - ubest;

    return wbest[0]? wbest:0;
}

//static string getExecutableFileFullName()
//{
//    char buf[MAX_PATH]; 
//
//    if( !GetModuleFileName( NULL, buf, MAX_PATH ) )
//        return string();
//
//    string path = buf;
//    replace( path.begin(), path.end(), '\\', '/' );
//    return path;
//}

static std::string getCallStack(
    HANDLE threadID,
    PEXCEPTION_RECORD exceptionRecord,
    PCONTEXT contextRecord )
{
    STACKFRAME64 sf;
    BOOL ok;
    PIMAGEHLP_SYMBOL64 pSym;
    IMAGEHLP_MODULE64 Mod;
    DWORD64 Disp;
    BYTE *bp;
    char *ou, *outstr;

    SymSetOptions( SYMOPT_UNDNAME );

    pSym = (PIMAGEHLP_SYMBOL64)GlobalAlloc( GMEM_FIXED, SYMSIZE+MAXTEXT );
    ou = outstr = ((char *)pSym)+SYMSIZE;
    *ou = '\0';
    if( exceptionRecord )
    {
        sprintf(ou, " at %08p:", bp = (BYTE *)(exceptionRecord->ExceptionAddress));
        try
        {
            for( int k = 0; k<12; ++k )
            {
                ou += strlen(ou);
                sprintf(ou, " %02x", *bp++);
            }
        }
        catch( unsigned int w )
        {
            /* Fails if bp is invalid address */
            w= 0;
            ou += strlen(ou);
            sprintf(ou, " Invalid Address");
        }
    }
    ou += strlen(ou);
    sprintf(ou, "\n");
    if( contextRecord )
        prregs( ou+strlen(ou), contextRecord );
    Mod.SizeOfStruct = sizeof(Mod);

    ZeroMemory(&sf, sizeof(sf));
#ifdef _WIN64
    //TODO/IMPL #ak
#else
    if( contextRecord )
    {
        sf.AddrPC.Offset = contextRecord->Eip;
        sf.AddrStack.Offset = contextRecord->Esp;
        sf.AddrFrame.Offset = contextRecord->Ebp;
    }
#endif
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrStack.Mode = AddrModeFlat;
    sf.AddrFrame.Mode = AddrModeFlat;

    ok = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    ou += strlen(ou);
    sprintf( ou, "Stack Trace:\n" );
    for( int i = 0; ; ++i )
    {
        ok = StackWalk64(
#ifndef _WIN64
            IMAGE_FILE_MACHINE_I386,
#else
            IMAGE_FILE_MACHINE_AMD64,
#endif
            GetCurrentProcess(),
            threadID,
            &sf,
            contextRecord,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL );

        if(!ok || sf.AddrFrame.Offset == 0)
        {
            if( i == 0 )
                sprintf( ou, "StackWalk64 failed\n ");
            break;
        }
        ou += strlen(ou);
        if( ou>outstr + MAXTEXT-1000 )
        {
            sprintf(ou, "No more room for trace\n");
            break;
        }
        sprintf(ou, "%08x:", sf.AddrPC.Offset);
        DWORD dwModBase = SymGetModuleBase64(GetCurrentProcess(), sf.AddrPC.Offset);
        ou += strlen(ou);
        if( dwModBase && SymGetModuleInfo64(GetCurrentProcess(), sf.AddrPC.Offset, &Mod) )
        {
            sprintf(ou, " %s", Mod.ModuleName);
            ou += strlen(ou);
        }
        pSym->SizeOfStruct = SYMSIZE;
        pSym->MaxNameLength = SYMSIZE - sizeof(IMAGEHLP_SYMBOL);
        if( SymGetSymFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &Disp, pSym) )
        {
            sprintf(ou, " %s() + 0x%x\n", pSym->Name, Disp);
        }
        //else if( dwModBase )
        //{
        //    sprintf(ou, ": 0x%x\n", sf.AddrPC.Offset - dwModBase);
        //}
        else
        {
            //generating map-file name
            QFileInfo exeFileInfo( QCoreApplication::applicationFilePath() );

            //std::string executableNameExt = getExecutableFileFullName();
            //// ex.: /usr/local/ipsoft/bin/radius
            //string executablePath;        // "/usr/local/ipsoft/bin"
            //string executableFullName;    // "radius"
            //string executableName;        // "radius"
            //string executableExt;        // ""
            //parsePath( executableNameExt, &executablePath, &executableFullName );
            //parseFileName( executableFullName, &executableName, &executableExt );
            //std::string mapFileName = executablePath + "/" + executableName + ".map";

            QString mapFileName = exeFileInfo.absoluteDir().absolutePath() + QLatin1String("/") + exeFileInfo.baseName() + QLatin1String(".map");

            char *cp = trmap( sf.AddrPC.Offset, mapFileName.toLatin1().constData(), &Disp );
            if( cp )
            {
                if( *cp == '?' )
                {
                    UnDecorateSymbolName( cp, pSym->Name, pSym->MaxNameLength, UNDNAME_COMPLETE );
                    sprintf(ou, " %s + 0x%x\n", pSym->Name, Disp);
                }
                else
                {
                    sprintf(ou, " %s() + 0x%x\n", cp, Disp);
                }
            }
        }
    }
    //printf("%s", outstr);
    SymCleanup(GetCurrentProcess());
    std::string out( outstr );
    GlobalFree(pSym);

    return out;
}

static const char* exceptToString( DWORD code )
{
    switch( code )
    {
    //case EXCEPTION_ACCESS_VIOLATION:
    //    return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
    //case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    //    return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
    //case EXCEPTION_BREAKPOINT: 
    //    return "A breakpoint was encountered.";
    //case EXCEPTION_DATATYPE_MISALIGNMENT:
    //    return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.";
    //case EXCEPTION_FLT_DENORMAL_OPERAND:
    //    return "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.";
    //case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    //    return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
    //case EXCEPTION_FLT_INEXACT_RESULT:
    //    return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
    //case EXCEPTION_FLT_INVALID_OPERATION:
    //    return "This exception represents any floating-point exception not included in this list.";
    //case EXCEPTION_FLT_OVERFLOW:
    //    return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
    //case EXCEPTION_FLT_STACK_CHECK:
    //    return "The stack overflowed or underflowed as the result of a floating-point operation.";
    //case EXCEPTION_FLT_UNDERFLOW:
    //    return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
    //case EXCEPTION_ILLEGAL_INSTRUCTION:
    //    return "The thread tried to execute an invalid instruction.";
    //case EXCEPTION_IN_PAGE_ERROR:
    //    return "The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.";
    //case EXCEPTION_INT_DIVIDE_BY_ZERO:
    //    return "The thread tried to divide an integer value by an integer divisor of zero.";
    //case EXCEPTION_INT_OVERFLOW:
    //    return "The result of an integer operation caused a carry out of the most significant bit of the result.";
    //case EXCEPTION_INVALID_DISPOSITION:
    //    return "An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception.";
    //case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    //    return "The thread tried to continue execution after a noncontinuable exception occurred.";
    //case EXCEPTION_PRIV_INSTRUCTION:
    //    return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
    //case EXCEPTION_SINGLE_STEP:
    //    return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
    //case EXCEPTION_STACK_OVERFLOW:
    //    return "The thread used up its stack.";
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION.";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED.";
    case EXCEPTION_BREAKPOINT: 
        return "EXCEPTION_BREAKPOINT.";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT.";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND.";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO.";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "EXCEPTION_FLT_INEXACT_RESULT.";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION.";
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW.";
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK.";
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW.";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION.";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR.";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO.";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW.";
    case EXCEPTION_INVALID_DISPOSITION:
        return "EXCEPTION_INVALID_DISPOSITION.";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION.";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION.";
    case EXCEPTION_SINGLE_STEP:
        return "EXCEPTION_SINGLE_STEP.";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW.";
    default:
        return "Unknown exception";
    }
}

win32_exception::win32_exception( PEXCEPTION_POINTERS info )
: 
    mWhere( info->ExceptionRecord->ExceptionAddress ),
    mCode( info->ExceptionRecord->ExceptionCode )
{
    std::ostringstream oss;

    oss << std::string( "WIN32 SYSTEM EXCEPTION. " ) << exceptToString( info->ExceptionRecord->ExceptionCode );
    oss << "\nCall stack:\n";
    oss << getCallStack( GetCurrentThread(), info->ExceptionRecord, info->ContextRecord );

    oss << "ExceptionCode: " << info->ExceptionRecord->ExceptionCode << ".";
    oss << " ExceptionFlags: " << info->ExceptionRecord->ExceptionFlags << ".";
    oss << " ExceptionAddress: 0x" << std::hex<< (uint32_t)info->ExceptionRecord->ExceptionAddress << ".";
    oss << " NumberParameters: " << info->ExceptionRecord->NumberParameters << ".";

    m_what = oss.str();
}

access_violation::access_violation( PEXCEPTION_POINTERS info )
: 
    win32_exception( info ),
    mIsWrite( false ),
    mBadAddress( 0 )
{
    mIsWrite = info->ExceptionRecord->ExceptionInformation[0] == 1;
    mBadAddress = reinterpret_cast<win32_exception::Address>(info->ExceptionRecord->ExceptionInformation[1]);
    std::ostringstream oss;
    oss << " BadAddress: 0x" <<std::hex<< (uint32_t)info->ExceptionRecord->ExceptionAddress << ".";
}

#endif
