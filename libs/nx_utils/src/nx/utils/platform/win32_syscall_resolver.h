/**********************************************************
* 17 oct 2014
* a.kolesnikov
***********************************************************/

#pragma once

#ifdef _WIN32

#include <list>
#include <string>

struct Win32FuncResolverImpl;

/*!
    NOTE: Caches all functions it had resolved
*/
class NX_UTILS_API Win32FuncResolver
{
public:
    Win32FuncResolver();
    ~Win32FuncResolver();

    /*!
        NOTE: thread-safe
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
    Win32FuncResolverImpl* m_impl;

    void* resolveFunction( const std::wstring& libName, const std::string& funcName );
};

#endif
