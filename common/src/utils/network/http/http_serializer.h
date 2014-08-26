/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_SERIALIZER_H
#define HTTP_SERIALIZER_H

#include "httptypes.h"
#include "../buffer.h"


namespace nx_http
{
    namespace SerializerState
    {
        typedef int Type;

        static const int needMoreBufferSpace = 1;
        static const int done = 2;
    };

    class MessageSerializer
    {
    public:
        MessageSerializer();

        //!Set message to serialize
        void setMessage(const Message& message);
        
        SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );

    private:
        const Message* m_message;
    };
}

#endif  //HTTP_SERIALIZER_H
