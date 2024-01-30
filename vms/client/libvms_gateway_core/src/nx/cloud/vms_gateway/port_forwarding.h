// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <set>
#include <vector>

#include <nx/network/socket_factory.h>
#include <nx/utils/thread/mutex.h>

#include "run_time_options.h"

namespace nx::cloud::gateway {

class PortForwarding final
{
public:
    PortForwarding(const conf::RunTimeOptions& runTimeOptions);
    ~PortForwarding();

    std::map<uint16_t, uint16_t> forward(
        const nx::network::SocketAddress& target,
        const std::set<uint16_t>& targetPorts,
        const nx::network::ssl::AdapterFunc& certificateCheck);

private:
    class Server;

private:
    const conf::RunTimeOptions& m_runTimeOptions;
    nx::Mutex m_mutex;
    std::map<nx::network::SocketAddress, std::map<uint16_t, std::unique_ptr<Server>>> m_servers;
};

} // namespace nx::cloud::gateway
