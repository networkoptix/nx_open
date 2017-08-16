#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

class AbstractListenAddressHelperHandler
{
public:
    virtual void onBestEndpointDiscovered(std::string publicAddress) = 0;
    virtual ~AbstractListenAddressHelperHandler() = default;
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx