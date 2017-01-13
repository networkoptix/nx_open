#include "pollset_factory.h"

#include <nx/utils/std/cpp14.h>

#include "pollset.h"
#include "unified_pollset.h"
#include "pollset_wrapper.h"

namespace nx {
namespace network {
namespace aio {

std::unique_ptr<AbstractPollSet> PollSetFactory::create()
{
    return std::make_unique<PollSetWrapper<UnifiedPollSet>>();
}

PollSetFactory* PollSetFactory::instance()
{
    static PollSetFactory factory;
    return &factory;
}

} // namespace aio
} // namespace network
} // namespace nx
