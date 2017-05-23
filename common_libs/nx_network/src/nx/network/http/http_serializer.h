#pragma once 

#include "http_types.h"
#include "../buffer.h"

namespace nx_http {

namespace SerializerState
{
    typedef int Type;

    static const int needMoreBufferSpace = 1;
    static const int done = 2;
};

class NX_NETWORK_API MessageSerializer
{
public:
    MessageSerializer();

    //!Set message to serialize
    void setMessage(const Message* message);
        
    SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );

private:
    const Message* m_message;
};

} // namespace nx_http
