/**********************************************************
* 17 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef WIN32_SYSCALL_RESOLVER_H
#define WIN32_SYSCALL_RESOLVER_H

#ifdef _WIN32

#include <list>
#include <mutex>
#include <string>


/*!
    \note Caches all functions it had resolved
*/
class Win32FuncResolver
{
public:
    Win32FuncResolver();
    ~Win32FuncResolver();

    /*!
        \note thread-safe
    */
    template<class FuncPointerType>
        FuncPointerType resolveFunction(
            const std::wstring& libName,
            const std::string& funcName,
            FuncPointerType defaultValue = nullptr )
    {
        FuncPointerType func = (FuncPointerType)resolveFunction( libName, funcName );
        return func ? func : defaultValue;
    }

    static Win32FuncResolver* instance();

private:
    class LibFunctions
    {
    public:
        HMODULE hLib;
        std::map<std::string, void*> funcByName;

        LibFunctions()
        :
            hLib( NULL )
        {
        }
    };

    std::mutex m_mutex;
    //!map<libname, function addresses>
    std::map<std::wstring, LibFunctions> m_loadedLibraries;

    void* resolveFunction( const std::wstring& libName, const std::string& funcName );
};

#endif

#endif  //WIN32_SYSCALL_RESOLVER_H
