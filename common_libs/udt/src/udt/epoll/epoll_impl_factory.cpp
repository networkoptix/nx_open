#include "epoll_impl_factory.h"

#if defined(_WIN32)
#include "epoll_impl_win32.h"
#elif defined(__linux__)
#include "epoll_impl_linux.h"
#elif defined(__APPLE__)
#include "epoll_impl_macosx.h"
#endif

std::unique_ptr<EpollImpl> EpollImplFactory::create()
{
#ifdef _WIN32
    return std::unique_ptr<EpollImpl>(new CEPollDescWin32());
#elif defined(__linux__)
    return std::unique_ptr<EpollImpl>(new CEPollDescLinux());
#elif defined(__APPLE__)
    return std::unique_ptr<EpollImpl>(new CEPollDescMacosx());
#else
    #error "Missing epoll implementation for the current platform"
#endif
}

EpollImplFactory* EpollImplFactory::instance()
{
    static EpollImplFactory s_instance;
    return &s_instance;
}
