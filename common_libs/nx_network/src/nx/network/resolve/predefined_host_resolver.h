#pragma once

#include <map>
#include <vector>

#include <nx/utils/thread/mutex.h>

#include "abstract_resolver.h"

namespace nx {
namespace network {

class NX_NETWORK_API PredefinedHostResolver:
    public AbstractResolver
{
public:
    virtual std::deque<HostAddress> resolve(const QString& name, int ipVersion = 0) override;

    void addEtcHost(const QString& name, std::vector<HostAddress> addresses);
    void removeEtcHost(const QString& name);

private:
    mutable QnMutex m_mutex;
    std::map<QString, std::vector<HostAddress>> m_etcHosts;
};

} // namespace network
} // namespace nx
