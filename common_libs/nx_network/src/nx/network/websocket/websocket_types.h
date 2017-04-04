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

using ConnectionMessageManagerType = 
    websocketpp::config::core::con_msg_manager_type<
        ws::message_buffer::message<ConnectionMessageManagerType>
    >;

using ConnectionMessageManagerTypePtr = ConnectionMessageManagerType::ptr;

using ProcessorBaseType = ws::processor::processor<websocketpp::config::core>;
using ProcessorBaseTypePtr = std::unique_ptr<ProcessorBaseType>;
using ProcessorHybi13Type = ws::processor::hybi13<websocketpp::config::core>;

using RandomGenType = websocketpp::config::core::rng_type;

//using MessageImplTypePtr = ProcessorImplType::message_ptr;

}
}
}