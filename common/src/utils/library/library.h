#ifndef __LIBRARY_H__
#define __LIBRARY_H__

#if defined(_WIN32)
# if !defined(_WINDOWS_)
#   include <windows.h>
# endif
#else
# include <dlfcn.h>
#endif

#include <string>
#include <utility>

class QnLibrary
{
public:
#if defined(_WIN32)
    typedef HMODULE SystemType;
#else
    typedef void* SystemType;
#endif
    typedef void* SymbolPointer;
   
private: //non-copyable
    QnLibrary(const QnLibrary& other);
    QnLibrary& operator =(const QnLibrary& other);

    QnLibrary(QnLibrary&& other);
    QnLibrary& operator =(QnLibrary&& other);
public: // CTOR/DTOR
    explicit
        QnLibrary(char const* name)
            : hlib_(QnLibrary::load(name))
    {}
        
    ~QnLibrary()
    {
        if(this->ok())
        {
            QnLibrary::free(hlib_);
        }
    }
public:
    SymbolPointer symbol(char const* name) const
    {
        return QnLibrary::symbolAddress(hlib_, name);
    }

    template<
        typename funcT
    >
    funcT symbolAs(char const* name) const
    {
        return funcT(this->symbol(name));
    }

    static
    std::string strerror();
public: // state checkers
    SystemType get() const
    {
        return hlib_;
    }

    bool ok() const
    {
        return hlib_ != invalid_handle();
    }

    bool operator!() const
    {
        return !this->ok();
    }
private:
    static
        SystemType invalid_handle()
    {
        return 0;
    }

    static
    SystemType load(char const* name);

    static
    SymbolPointer symbolAddress(SystemType hlib, char const* name);

    static
    bool free(SystemType hlib);
private:
    SystemType hlib_;
}; // class <library>

inline
QnLibrary::SystemType QnLibrary::load(char const* name)
{
#if defined(_WIN32)
    //get current error mode flags
    UINT error_mode = ::SetErrorMode(SEM_NOGPFAULTERRORBOX);
    // disable critical-error-handler message box
    ::SetErrorMode(error_mode | SEM_FAILCRITICALERRORS);
    // clear any existing error
    ::SetLastError(ERROR_SUCCESS);
    // load library by name
    SystemType hlib = ::LoadLibraryA(name);
    // restore error mode flags...
    ::SetErrorMode(error_mode);
    return hlib;
#else
    // clear any existing error
    ::dlerror();
    // load library by name
    return ::dlopen(name, RTLD_LAZY);
#endif
}

inline
QnLibrary::SymbolPointer
QnLibrary::symbolAddress(SystemType hlib, char const* name)
{
#if defined(_WIN32)
    // clear any existing error
    ::SetLastError(ERROR_SUCCESS);
    // request object symbol by name
    return ::GetProcAddress(hlib, name);
#else
    // clear any existing error
    ::dlerror();
    // request object symbol by name
    return ::dlsym(hlib, name);
#endif
}

inline
bool QnLibrary::free(SystemType hlib)
{
#if defined(_WIN32)
    // clear any existing error
    ::SetLastError(ERROR_SUCCESS);
    // free library
    return ::FreeLibrary(hlib) != TRUE;
#else
    // clear any existing error
    ::dlerror();
    // free library handle
    return ::dlclose(hlib) == 0;
#endif
}

inline
std::string QnLibrary::strerror()
{
#if defined(_WIN32)
    typedef char* char_pointer;

    DWORD ecode = ::GetLastError();
    std::string etext;
    if(ecode != ERROR_SUCCESS)
    {
        void* xdata = 0;
        DWORD xsize = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
            0,                                          // source
            ecode,                                      // error code
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // language identifier
            char_pointer(&xdata),                       // buffer
            0,                                          // size
            0                                           // arguments
            );
        if(xdata && xsize > 0)
        {
            if(char_pointer(xdata)[xsize - 2] == '\r' &&
                char_pointer(xdata)[xsize - 1] == '\n')
            {
                (char_pointer(xdata))[xsize - 2] = '\0';
            }
            etext.assign(char_pointer(xdata));
        }
    }
    return (etext.empty() ? "<no-error>" : etext);
#else
    char const* c_etext = ::dlerror();
    if(c_etext)
    {
        return c_etext;
    }
    return "<no-error>";
#endif
}

#endif // _SYS_LIBRARY_INCLUDED_
