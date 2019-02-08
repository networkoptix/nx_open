#pragma once

#include <memory>

#include "abstract_epoll.h"

class EpollFactory
{
public:
    std::unique_ptr<AbstractEpoll> create();

    static EpollFactory* instance();
};
