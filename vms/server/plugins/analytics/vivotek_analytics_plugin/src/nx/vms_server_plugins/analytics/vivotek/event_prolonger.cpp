#include "event_prolonger.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::utils;

namespace {

} // namespace

void EventProlonger::write(nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket> packet)
{
    m_packets.push_back(std::move(packet));

    pump();
}

cf::future<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> EventProlonger::read()
{
    auto future = m_promise.emplace().get_future()
        .then(cf::translate_broken_promise_to_operation_canceled);

    pump();

    return future;
}

void EventProlonger::pump()
{
    if (m_promise && !m_packets.empty())
    {
        auto promise = std::move(*m_promise);
        m_promise.reset();

        auto packet = std::move(m_packets.front());
        m_packets.pop_front();

        promise.set_value(std::move(packet));
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
