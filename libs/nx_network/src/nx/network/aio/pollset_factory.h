// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {

class NX_NETWORK_API PollSetFactory
{
public:
    PollSetFactory();
    ~PollSetFactory();

    std::unique_ptr<AbstractPollSet> create();

    void enableUdt();
    void disableUdt();

    static PollSetFactory* instance();

private:
    bool m_udtEnabled;
};

} // namespace aio
} // namespace network
} // namespace nx
