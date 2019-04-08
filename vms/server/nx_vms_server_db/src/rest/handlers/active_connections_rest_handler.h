#pragma once

#include <nx/network/rest/handler.h>

namespace ec2 { class AbstractTransactionMessageBus; }

namespace rest {
namespace handlers {

class ActiveConnectionsRestHandler: public nx::network::rest::Handler
{
public:
    ActiveConnectionsRestHandler(const ec2::AbstractTransactionMessageBus* messageBus);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

private:
    const ec2::AbstractTransactionMessageBus* m_messageBus;
};

} // namespace handlers
} // namespace rest
