
#ifdef _WIN32

#ifndef SYSTEMEXCEPT_WIN32_H
#define SYSTEMEXCEPT_WIN32_H

#include <exception>
#include <string>
#include <windows.h>


//!Used to dump unhandled structured exception call stack to file ({application_name}_{pid}.except near application binary)
/*!
    \note This does not work if structured exception handling is not used and application is being debugged
        (debugger handles unhandled exceptions by itself)
    \note This handling works on per-thread basis. \a win32_exception::translate has to be called in thread to enable unhandled structured exception handling
    \note QnLongRunnable instance calls \a win32_exception::translate after thread creation
    \todo some refactoring is required
*/
class win32_exception
: 
    public std::exception
{
public:
    typedef const void *Address; 

    //!Registers handler to intercept system exceptions (e.g., Access violation)
    static void installGlobalUnhandledExceptionHandler();
    static void installThreadSpecificUnhandledExceptionHandler();
    virtual const char* what() const;
    Address where() const;
    unsigned code() const;

    static void translate( 
        unsigned int code, 
        PEXCEPTION_POINTERS info );

protected:
    std::string m_what;

    win32_exception( PEXCEPTION_POINTERS info );


private:
    Address mWhere;
    unsigned mCode;
};

class access_violation
: 
    public win32_exception
{
public:
    bool isWrite() const;
    Address badAddress() const;

private:
    bool mIsWrite;
    Address mBadAddress;
    access_violation( PEXCEPTION_POINTERS info );
    friend void win32_exception::translate( 
        unsigned int code, 
        PEXCEPTION_POINTERS info );
};

#endif    //SYSTEMEXCEPT_WIN32_H

#endif
