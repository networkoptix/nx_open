
#ifdef _WIN32

#include "systemexcept_win32.h"

#include <fstream>
#include <sstream>
#include <string>

#include <Dbghelp.h>


using std::string;

//typedef struct STACK_FRAME
//{
//	STACK_FRAME* Ebp;   //address of the calling function frame
//	PBYTE Ret_Addr;      //return address
//	DWORD Param[0];      //parameter list - could be empty
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

static LONG WINAPI unhandledSEHandler( __in struct _EXCEPTION_POINTERS* ExceptionInfo )
{
    win32_exception::translate( EXCEPTION_ACCESS_VIOLATION, ExceptionInfo );
    return EXCEPTION_EXECUTE_HANDLER;
}

void win32_exception::install_handler()
{
	_set_se_translator( &win32_exception::translate );
    SetUnhandledExceptionFilter( &unhandledSEHandler );
}

void win32_exception::translate(
	unsigned int code, 
	PEXCEPTION_POINTERS info )
{
    const QFileInfo exeFileInfo( QCoreApplication::applicationFilePath() );
    const QString exceptFileName = QString::fromLatin1("%1/%2_%3.except").arg(exeFileInfo.absoluteDir().absolutePath()).arg(exeFileInfo.baseName()).arg(GetCurrentProcessId());

    switch( code )
	{
	    case EXCEPTION_ACCESS_VIOLATION:
		{
            std::ofstream of( exceptFileName.toLatin1().constData() );
			of<<"EXCEPTION_ACCESS_VIOLATION\n";
			access_violation e( info );
			of.write( e.what(), strlen(e.what()) );
			of.close();
			TerminateProcess( GetCurrentProcess(), 1 );
		    break;
		}
    
		default:
		{
			std::ofstream of( exceptFileName.toLatin1().constData() );
			of<<"STRUCTURED EXCEPTION\n";
			win32_exception e( info );
			of.write( e.what(), strlen(e.what()) );
			of.close();
			TerminateProcess( GetCurrentProcess(), 1 );
			break;
		}
    }
}

#define SYMSIZE 10000
#define MAXTEXT 5000

static char *prregs( char *str, PCONTEXT cr )
{
	char *cp= str;
	sprintf(cp, "\t%s:%08x", "Eax", cr->Eax); cp += strlen(cp);
	sprintf(cp, " %s:%08x",  "Ebx", cr->Ebx); cp += strlen(cp);
	sprintf(cp, " %s:%08x",  "Ecx", cr->Ecx); cp += strlen(cp);
	sprintf(cp, " %s:%08x\n","Edx", cr->Edx); cp += strlen(cp);
	sprintf(cp, "\t%s:%08x", "Esi", cr->Esi); cp += strlen(cp);
	sprintf(cp, " %s:%08x",  "Edi", cr->Edi); cp += strlen(cp);
	sprintf(cp, " %s:%08x",  "Esp", cr->Esp); cp += strlen(cp);
	sprintf(cp, " %s:%08x\n","Ebp", cr->Ebp); cp += strlen(cp);
	
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
//	char buf[MAX_PATH]; 
//
//	if( !GetModuleFileName( NULL, buf, MAX_PATH ) )
//		return string();
//
//	string path = buf;
//	replace( path.begin(), path.end(), '\\', '/' );
//	return path;
//}

static std::string getCallStack( PEXCEPTION_POINTERS pException )
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
	sprintf(ou, " at %08p:", bp = (BYTE *)(pException->ExceptionRecord->ExceptionAddress));
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
	ou += strlen(ou);
	sprintf(ou, "\n");
	prregs( ou+strlen(ou), pException->ContextRecord );
	Mod.SizeOfStruct = sizeof(Mod);

	ZeroMemory(&sf, sizeof(sf));
	sf.AddrPC.Offset = pException->ContextRecord->Eip;
	sf.AddrStack.Offset = pException->ContextRecord->Esp;
	sf.AddrFrame.Offset = pException->ContextRecord->Ebp;
	sf.AddrPC.Mode = AddrModeFlat;
	sf.AddrStack.Mode = AddrModeFlat;
	sf.AddrFrame.Mode = AddrModeFlat;

	ok = SymInitialize(GetCurrentProcess(), NULL, TRUE);
	ou += strlen(ou);
	sprintf( ou, "Stack Trace:\n" );
	for( ;; )
	{
		ok = StackWalk64(
			IMAGE_FILE_MACHINE_I386,
			GetCurrentProcess(),
			GetCurrentThread(),
			&sf,
			pException->ContextRecord,
			NULL,
			SymFunctionTableAccess64,
			SymGetModuleBase64,
			NULL );

		if(!ok || sf.AddrFrame.Offset == 0)
			break;
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
		//	sprintf(ou, ": 0x%x\n", sf.AddrPC.Offset - dwModBase);
		//}
		else
		{
			//generating map-file name
            QFileInfo exeFileInfo( QCoreApplication::applicationFilePath() );

            //std::string executableNameExt = getExecutableFileFullName();
			//// ex.: /usr/local/ipsoft/bin/radius
			//string executablePath;		// "/usr/local/ipsoft/bin"
			//string executableFullName;	// "radius"
			//string executableName;		// "radius"
			//string executableExt;		// ""
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
	//	return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
	//case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	//	return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
	//case EXCEPTION_BREAKPOINT: 
	//	return "A breakpoint was encountered.";
	//case EXCEPTION_DATATYPE_MISALIGNMENT:
	//	return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.";
	//case EXCEPTION_FLT_DENORMAL_OPERAND:
	//	return "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.";
	//case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	//	return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
	//case EXCEPTION_FLT_INEXACT_RESULT:
	//	return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
	//case EXCEPTION_FLT_INVALID_OPERATION:
	//	return "This exception represents any floating-point exception not included in this list.";
	//case EXCEPTION_FLT_OVERFLOW:
	//	return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
	//case EXCEPTION_FLT_STACK_CHECK:
	//	return "The stack overflowed or underflowed as the result of a floating-point operation.";
	//case EXCEPTION_FLT_UNDERFLOW:
	//	return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
	//case EXCEPTION_ILLEGAL_INSTRUCTION:
	//	return "The thread tried to execute an invalid instruction.";
	//case EXCEPTION_IN_PAGE_ERROR:
	//	return "The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.";
	//case EXCEPTION_INT_DIVIDE_BY_ZERO:
	//	return "The thread tried to divide an integer value by an integer divisor of zero.";
	//case EXCEPTION_INT_OVERFLOW:
	//	return "The result of an integer operation caused a carry out of the most significant bit of the result.";
	//case EXCEPTION_INVALID_DISPOSITION:
	//	return "An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception.";
	//case EXCEPTION_NONCONTINUABLE_EXCEPTION:
	//	return "The thread tried to continue execution after a noncontinuable exception occurred.";
	//case EXCEPTION_PRIV_INSTRUCTION:
	//	return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
	//case EXCEPTION_SINGLE_STEP:
	//	return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
	//case EXCEPTION_STACK_OVERFLOW:
	//	return "The thread used up its stack.";
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
	oss << getCallStack( info );

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
