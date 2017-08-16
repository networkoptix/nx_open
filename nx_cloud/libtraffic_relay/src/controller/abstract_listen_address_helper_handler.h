#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

class AbstractListenAddressHelperHandler
{
public:
    virtual void onPublicAddressDiscovered(std::string publicAddress) = 0;
    virtual ~AbstractListenAddressHelperHandler() = default;
};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx