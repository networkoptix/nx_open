#pragma once

#include <nx/network/buffer.h>
#include <nx/network/websocket/websocket_types.h>

namespace nx {
namespace network {
namespace websocket {

class Message
{
public:
    Message(
        ProcessorImplType* processorImpl,
        ConnectionMessageManagerImplType<MessageImplType>* conMessageManager,
        ws::frame::opcode::value operationCode, 
        const void* payload, 
        std::size_t payloadLen);
    void serialize(nx::Buffer* buf, std::size_t* bytesWritten);

private:
    ProcessorImplType* m_processor;
    MessageImplTypePtr m_message;
};

}
}
}