/**********************************************************
* 17 oct 2014
* a.kolesnikov
***********************************************************/

#ifdef _WIN32

#include "win32_syscall_resolver.h"

#include <memory>
#include <mutex>


Win32FuncResolver::Win32FuncResolver()
{
}

Win32FuncResolver::~Win32FuncResolver()
{
    for( auto lib: m_loadedLibraries )
        FreeLibrary( lib.second.hLib );
    m_loadedLibraries.clear();
}

static std::unique_ptr<Win32FuncResolver> Win32FuncResolver_instance;
static std::once_flag Win32FuncResolver_onceFlag;

Win32FuncResolver* Win32FuncResolver::instance()
{
    std::call_once(
        Win32FuncResolver_onceFlag,
        [](){ Win32FuncResolver_instance.reset( new Win32FuncResolver() ); } );
    return Win32FuncResolver_instance.get();
}

void* Win32FuncResolver::resolveFunction( const std::wstring& libName, const std::string& funcName )
{
    std::unique_lock<std::mutex> lk( m_mutex );

    auto libInsRes = m_loadedLibraries.insert( std::make_pair( libName, LibFunctions() ) );
    LibFunctions& libCtx = libInsRes.first->second;
    if( libInsRes.second )  //added new value
        libCtx.hLib = LoadLibrary( libName.c_str() );
    //not checking hLib for NULL to cache information that funcName from libName is not available

    auto funcInsRes = libCtx.funcByName.insert( std::make_pair( funcName, NULL ) );
    void*& funcAddr = funcInsRes.first->second;
    if( !funcInsRes.second ||       //function address already known (e.g., we know it is NULL)
        !libCtx.hLib )              //no module - no sense to search for func
    {
        return funcAddr;
    }
    
    funcAddr = GetProcAddress( libCtx.hLib, funcName.c_str() );
    return funcAddr;
}

#endif
