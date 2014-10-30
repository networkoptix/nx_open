
#ifdef _WIN32

#include "systemexcept_win32.h"

#include <fstream>
#include <sstream>
#include <string>

#include <Dbghelp.h>
#include <Windows.h>
#include <ShlObj.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>

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
void writeMiniDump( PEXCEPTION_POINTERS ex ) {
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

    ::MiniDumpWriteDump( 
        ::GetCurrentProcess(),
        ::GetCurrentProcessId(),
        hFile,
        sMDumpType,
        ex == NULL ? NULL : &sMDumpExcept,
        NULL,
        NULL);

    ::CloseHandle(hFile);
}

static void translate( unsigned int code , _EXCEPTION_POINTERS* ExceptionInfo ) {
    Q_UNUSED(code);
    writeMiniDump(ExceptionInfo);
    TerminateProcess( GetCurrentProcess(), 1 );
}

static LONG WINAPI unhandledSEHandler( __in struct _EXCEPTION_POINTERS* ExceptionInfo )
{
    translate(0,ExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

#if 0
static void writeCrashInfo(
    const char* title,
    const char* information )
{
    // The following code will try my best to avoid (to my best knowledge)
    // 1) Heap allocation ( 1. Heap corruption 2. CRT global lock )
    // 2) Function that cannot be re-entered
    // 3) Function that acquires global lock ( most of CRT function )
    char sProcessName[1024];
    char sFileName[1024];
    char sAppData[MAX_PATH];

    if( !GetProgramName(sProcessName) )
        return;
    
    if( FAILED(SHGetFolderPathA(
            NULL,
            CSIDL_LOCAL_APPDATA,
            NULL,
            0,
            sAppData)))
        return;

    int ret = sprintf(sFileName,"%s\\%s_%d.except", sAppData, sProcessName, ::GetCurrentProcessId());

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

done:
    ::CloseHandle(hFile);
}
#endif 

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

    writeMiniDump(NULL);
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

#if 0

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
#endif 
#endif
