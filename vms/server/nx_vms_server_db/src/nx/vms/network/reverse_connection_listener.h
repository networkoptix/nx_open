#pragma once

#include <network/http_connection_listener.h>

namespace nx {
namespace vms {
namespace network {

class ReverseConnectionManager;

class ReverseConnectionListener: public QnTCPConnectionProcessor
{
public:
    ReverseConnectionListener(
        ReverseConnectionManager* reverseConnectionManager,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);
protected:
    virtual void run();
private:
    nx::vms::network::ReverseConnectionManager* m_reverseConnectionManager = nullptr;
};

} // namespace network
} // namespace vms
} // namespace nx
