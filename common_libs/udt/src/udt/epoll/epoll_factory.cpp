#include "epoll_factory.h"

#if defined(_WIN32)
#include "epoll_win32.h"
#elif defined(__linux__)
#include "epoll_linux.h"
#elif defined(__APPLE__)
#include "epoll_macosx.h"
#endif

std::unique_ptr<AbstractEpoll> EpollFactory::create()
{
#ifdef _WIN32
    return std::unique_ptr<AbstractEpoll>(new EpollWin32());
#elif defined(__linux__)
    return std::unique_ptr<AbstractEpoll>(new EpollLinux());
#elif defined(__APPLE__)
    return std::unique_ptr<AbstractEpoll>(new EpollMacosx());
#else
    #error "Missing epoll implementation for the current platform"
#endif
}

EpollFactory* EpollFactory::instance()
{
    static EpollFactory s_instance;
    return &s_instance;
}
