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

namespace sys
{
    class library
    {
    public:
#if defined(_WIN32)
        typedef HMODULE system_type;
#else
        typedef void* system_type;
#endif
        typedef void* symbol_pointer;
   
    private: //non-copyable
        library(const library& other);
        library& operator =(const library& other);

        library(library&& other);
        library& operator =(library&& other);
    public: // CTOR/DTOR
        explicit
            library(char const* name)
                : hlib_(library::load(name))
        {}
        
        ~library()
        {
            if(this->ok())
            {
                library::free(hlib_);
            }
        }
    public:
        symbol_pointer symbol(char const* name) const
        {
            return library::symbol_address(hlib_, name);
        }

        template<
            typename funcT
        >
        funcT symbol_as(char const* name) const
        {
            return funcT(this->symbol(name));
        }

        static
        std::string strerror();
    public: // state checkers
        system_type get() const
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
            system_type invalid_handle()
        {
            return 0;
        }

        static
        system_type load(char const* name);

        static
        symbol_pointer symbol_address(system_type hlib, char const* name);

        static
        bool free(system_type hlib);
    private:
        system_type hlib_;
    }; // class <library>

    inline
    library::system_type library::load(char const* name)
    {
#if defined(_WIN32)
        //get current error mode flags
        UINT error_mode = ::SetErrorMode(SEM_NOGPFAULTERRORBOX);
        // disable critical-error-handler message box
        ::SetErrorMode(error_mode | SEM_FAILCRITICALERRORS);
        // clear any existing error
        ::SetLastError(ERROR_SUCCESS);
        // load library by name
        system_type hlib = ::LoadLibraryA(name);
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
    library::symbol_pointer
    library::symbol_address(system_type hlib, char const* name)
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
    bool library::free(system_type hlib)
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
    std::string library::strerror()
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
} // namespace <sys>

#endif // _SYS_LIBRARY_INCLUDED_
