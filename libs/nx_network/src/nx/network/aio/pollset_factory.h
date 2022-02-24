// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/singleton.h>

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {

class NX_NETWORK_API PollSetFactory:
    public Singleton<PollSetFactory>
{
public:
    PollSetFactory();

    std::unique_ptr<AbstractPollSet> create();

    void enableUdt();
    void disableUdt();

private:
    bool m_udtEnabled;
};

} // namespace aio
} // namespace network
} // namespace nx

// Workaround for the bug in the clang that doesn't look up for the instantiation of s_instance in
// the same library (prevent -Wundefined-var-template).
#if defined(__clang__)
    template<>
    nx::network::aio::PollSetFactory* Singleton<nx::network::aio::PollSetFactory>::s_instance;
#endif
