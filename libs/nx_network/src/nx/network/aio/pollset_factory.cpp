// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pollset_factory.h"

#include <nx/utils/std/cpp14.h>

#include "pollset.h"
#include "pollset_wrapper.h"
#include "unified_pollset.h"

template<>
nx::network::aio::PollSetFactory* Singleton<nx::network::aio::PollSetFactory>::s_instance =
    nullptr;

namespace nx {
namespace network {
namespace aio {

PollSetFactory::PollSetFactory():
    m_udtEnabled(true)
{
}

std::unique_ptr<AbstractPollSet> PollSetFactory::create()
{
    if (m_udtEnabled)
        return std::make_unique<PollSetWrapper<UnifiedPollSet>>();
    else
        return std::make_unique<PollSetWrapper<PollSet>>();
}

void PollSetFactory::enableUdt()
{
    m_udtEnabled = true;
}

void PollSetFactory::disableUdt()
{
    m_udtEnabled = false;
}

} // namespace aio
} // namespace network
} // namespace nx
