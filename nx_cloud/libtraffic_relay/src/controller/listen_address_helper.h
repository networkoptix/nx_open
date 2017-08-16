#pragma once

#include <string>
#include <boost/optional/optional.hpp>
#include "abstract_listen_address_helper_handler.h"

namespace nx {
namespace cloud {
namespace relay {
namespace controller {

class AbstractListenAddressHelper
{
public:
    AbstractListenAddressHelper(AbstractListenAddressHelperHandler* handler);
    virtual ~AbstractListenAddressHelper() = default;

    void findBestAddress(const std::vector<SocketAddress>& httpEndpoints) const;

private:
    AbstractListenAddressHelperHandler* m_handler;

    void doReport(
        const SocketAddress& address,
        const lm& message,
        utils::log::Level logLevel) const;
    bool endpointReportedAsPublic(
        const SocketAddress& listenAddress,
        const HostAddress& publicAddress) const;

    virtual boost::optional<HostAddress> publicAddress() const = 0;
};

class ListenAddressHelper: public AbstractListenAddressHelper
{
public:
    ListenAddressHelper(AbstractListenAddressHelperHandler* handler);

private:
    boost::optional<HostAddress> m_publicAddress;

    virtual boost::optional<HostAddress> publicAddress() const override;

};

} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx