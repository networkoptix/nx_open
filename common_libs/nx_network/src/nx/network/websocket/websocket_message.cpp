#include <nx/network/websocket/websocket_message.h>

namespace nx {
namespace network {
namespace websocket {

Message::Message(
    ProcessorImplType* processorImpl,
    ConnectionMessageManagerImplType<MessageImplType>* conMessageManager,
    ws::frame::opcode::value operationCode,
    const void* payload,
    std::size_t payloadLen) 
    :
    m_processor(processorImpl),
    m_message(conMessageManager, operationCode)
{
    m_message.append_payload(payload, payloadLen);
}

void Message::serialize(nx::Buffer* buf, std::size_t* bytesWritten)
{
    MessageImplTypePtr out;
    m_processor->prepare()
}

}
}
}