#pragma once

#include <memory>

#include <nx/utils/singleton.h>

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {

class PollSetFactory:
    public Singleton<PollSetFactory>
{
public:
    PollSetFactory();

    std::unique_ptr<AbstractPollSet> create();

    void disableUdt();

private:
    bool m_udtEnabled;
};

} // namespace aio
} // namespace network
} // namespace nx
