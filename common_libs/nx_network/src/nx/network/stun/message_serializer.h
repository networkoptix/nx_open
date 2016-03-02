/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_SERIALIZER_H
#define STUN_MESSAGE_SERIALIZER_H

#include <cstdint>
#include <functional>
#include <nx/network/buffer.h>
#include <nx/utils/log/assert.h>

#include "message.h"

#include "../connection_server/base_protocol_message_types.h"


namespace nx {
namespace stun {

    //!Serializes message to buffer
    class NX_NETWORK_API MessageSerializer
    {
    public:
        MessageSerializer();

        //!Set message to serialize
        void setMessage(const Message* message);

        /** Serializes full message increasing \a buffer if needed.
            \param bytesWritten Serialized message length is written here
         */
        nx_api::SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );

        static nx::Buffer serialized(const Message& message);

    private:
        class MessageSerializerBuffer;
        // header serialization
        nx_api::SerializerState::Type serializeHeader( MessageSerializerBuffer* buffer );
        nx_api::SerializerState::Type serializeHeaderInitial( MessageSerializerBuffer* buffer );
        // We cannot serialize header before we finish serialization of the body/attributes
        // So this method is just make a mark internally.
        nx_api::SerializerState::Type serializeHeaderLengthStart ( MessageSerializerBuffer* buffer );
        // transaction id and magic cookie serialization
        nx_api::SerializerState::Type serializeMagicCookieAndTransactionID( MessageSerializerBuffer* buffer );
        // attribute serialization
        nx_api::SerializerState::Type serializeAttributes( MessageSerializerBuffer* buffer , std::uint16_t* length );
        nx_api::SerializerState::Type serializeAttributeTypeAndLength( MessageSerializerBuffer* buffer ,const attrs::Attribute* attribute , std::uint16_t** value_pos );
        nx_api::SerializerState::Type serializeAttributeValue( MessageSerializerBuffer* buffer ,const attrs::Attribute* attribute , std::size_t* value );
        // value 
        nx_api::SerializerState::Type serializeAttributeValue_XORMappedAddress( MessageSerializerBuffer* buffer ,const attrs::XorMappedAddress& , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_Fingerprint( MessageSerializerBuffer* buffer ,const attrs::FingerPrint& , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_ErrorCode( MessageSerializerBuffer* buffer ,const attrs::ErrorDescription& , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_Buffer( MessageSerializerBuffer* buffer ,const attrs::BufferedValue& , std::size_t* value );
        // this function will do minimum checking for the message object
        bool checkMessageIntegratiy();
        // This helper function ensure that while we traveling the attributes of a message, the MessageIntegrity and the
        // FingerPrint will be ensured to exist at last. The RFC doesn't restrict the order of the attributes, but I guess
        // the unorder_map is OK here, as long as we ensure the MessageIntegrity and FingerPrint message order .
        bool travelAllAttributes( const std::function<bool(const attrs::Attribute*)>& );
        std::size_t calculatePaddingSize( std::size_t size ) {
            static const std::size_t kAlignMask = 3;
            return (size + kAlignMask) & ~kAlignMask;
        }

    private:
        const Message* m_message;
        bool m_initialized;
    };

} // namespase stun
} // namespase nx

#endif  //STUN_MESSAGE_SERIALIZER_H
