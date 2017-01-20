#pragma once

#include <memory>

#include "epoll_impl.h"

class EpollImplFactory
{
public:
    std::unique_ptr<EpollImpl> create();

    static EpollImplFactory* instance();
};
