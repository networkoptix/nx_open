#pragma once

#include <memory>

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {

class PollSetFactory
{
public:
    std::unique_ptr<AbstractPollSet> create();

    static PollSetFactory* instance();
};

} // namespace aio
} // namespace network
} // namespace nx
