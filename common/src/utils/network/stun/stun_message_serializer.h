/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_SERIALIZER_H
#define STUN_MESSAGE_SERIALIZER_H

#include <cstdint>
#include <functional>
#include <utils/network/buffer.h>

#include "stun_message.h"
#include "../connection_server/base_protocol_message_types.h"


namespace nx_stun
{
    //!Serializes message to buffer
    class MessageSerializer
    {
    public:
        //!Set message to serialize
        void setMessage( Message&& message ) {
            message_ = std::move(message);
            initialized_ = true;
        }

        nx_api::SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );

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
        nx_api::SerializerState::Type serializeAttributeTypeAndLength( MessageSerializerBuffer* buffer ,const attr::Attribute* attribute , std::uint16_t** value_pos );
        nx_api::SerializerState::Type serializeAttributeValue( MessageSerializerBuffer* buffer ,const attr::Attribute* attribute , std::size_t* value );
        // value 
        nx_api::SerializerState::Type serializeAttributeValue_XORMappedAddress( MessageSerializerBuffer* buffer ,const attr::XorMappedAddress* , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_Fingerprint( MessageSerializerBuffer* buffer ,const attr::FingerPrint* , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_ErrorCode( MessageSerializerBuffer* buffer ,const attr::ErrorCode* , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_MessageIntegrity( MessageSerializerBuffer* buffer ,const attr::MessageIntegrity* , std::size_t* value );
        nx_api::SerializerState::Type serializeAttributeValue_UnknownAttribute( MessageSerializerBuffer* buffer ,const attr::UnknownAttribute* , std::size_t* value );
        // this function will do minimum checking for the message object
        bool checkMessageIntegratiy();
        // This helper function ensure that while we traveling the attributes of a message, the MessageIntegrity and the
        // FingerPrint will be ensured to exist at last. The RFC doesn't restrict the order of the attributes, but I guess
        // the unorder_map is OK here, as long as we ensure the MessageIntegrity and FingerPrint message order .
        bool travelAllAttributes( const std::function<bool(const attr::Attribute*)>& );
        std::size_t calculatePaddingSize( std::size_t size ) {
            static const std::size_t kAlignMask = 3;
            return (size + kAlignMask) & ~kAlignMask;
        }

    private:
        Message message_;
        bool initialized_;
    };

}

#endif  //STUN_MESSAGE_SERIALIZER_H
