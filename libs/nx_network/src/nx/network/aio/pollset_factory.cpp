// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pollset_factory.h"

#include <nx/utils/std/cpp14.h>

#include "pollset.h"
#include "pollset_wrapper.h"
#include "unified_pollset.h"

namespace nx {
namespace network {
namespace aio {

PollSetFactory* s_instance = nullptr;

PollSetFactory::PollSetFactory():
    m_udtEnabled(true)
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;
}

PollSetFactory::~PollSetFactory()
{
    if (s_instance == this)
        s_instance = nullptr;
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

PollSetFactory* PollSetFactory::instance()
{
    return s_instance;
}

} // namespace aio
} // namespace network
} // namespace nx
