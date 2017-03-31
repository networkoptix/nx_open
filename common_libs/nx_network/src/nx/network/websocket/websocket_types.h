#pragma once

#include <nx/network/websocket/websocketpp/message_buffer/message.hpp>
#include <nx/network/websocket/websocketpp/message_buffer/alloc.hpp>
#include <nx/network/websocket/websocketpp/frame.hpp>
#include <nx/network/websocket/websocketpp/processors/hybi13.hpp>
#include <nx/network/websocket/websocketpp/processors/hybi00.hpp>
#include <nx/network/websocket/websocketpp/processors/hybi07.hpp>
#include <nx/network/websocket/websocketpp/processors/hybi08.hpp>
#include <nx/network/websocket/websocketpp/config/core.hpp>
#include <nx/network/websocket/websocketpp/random/random_device.hpp>
#include <nx/network/websocket/websocketpp/concurrency/basic.hpp>


namespace nx {
namespace network {
namespace websocket {

namespace ws = websocketpp;

template <typename Message>
using ConnectionMessageManagerType = websocketpp::config::core::con_msg_manager_type;

template <typename Message>
using ConnectionMessageManagerTypePtr = ConnectionMessageManagerType::ptr;

using MessageType = ws::message_buffer::message<ConnectionMessageManagerType>;

using ProcessorBaseType = ws::processor::processor<websocketpp::config::core>;
using ProcessorBaseTypePtr = std::unique_ptr<ProcessorBaseType>;

using ProcessorHybi00Type = ws::processor::hybi00<websocketpp::config::core>;
using ProcessorHybi07Type = ws::processor::hybi07<websocketpp::config::core>;
using ProcessorHybi08Type = ws::processor::hybi08<websocketpp::config::core>;
using ProcessorHybi13Type = ws::processor::hybi13<websocketpp::config::core>;

using RandomGenType = websocketpp::config::core::rng_type;

//using MessageImplTypePtr = ProcessorImplType::message_ptr;

}
}
}