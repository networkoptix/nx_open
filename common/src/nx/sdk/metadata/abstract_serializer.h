#pragma once

#include <nx/sdk/common.h>
#include <nx/sdk/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractSerializator interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_Serializer
    = {{0xf1, 0xc8, 0xfc, 0x64, 0x03, 0x19, 0x47, 0x6f, 0xa9, 0xfd, 0xee, 0x24, 0x32, 0x27, 0xe9, 0xc7}};

/**
 * @brief The AbstractSerializator class is an interface for classes
 * providing data packet serialization / deserialization functionality.
 */
class AbstractSerializer: public nxpl::PluginInterface
{
    /**
     * @brief serialize serializes data packet.
     * @param packet packet to be serialized
     * @param buffer output serialized buffer
     * @param bufferSize size of buffer for serialization. If the size is not enough
     * then this method MUST return needMoreBufferSpace and set inOutBufferSize to the desired value.
     * @return noError in case of success,
     * needMoreBufferSpace if buffer is too small,
     * some other value otherwise.
     */
    virtual Error serialize(const AbstractDataPacket* packet, char* outBuffer, int inOutBufferSize) = 0;

    /**
     * @brief deserialize deserializes data packet
     * @param packet output deserialized packet
     * @param buffer buffer to be deserialized
     * @param bufferSize size of buffer
     * @return noError in case of success, some other value otherwise.
     */
    virtual Error deserialize(AbstractDataPacket** outPacket, const char* buffer, int bufferSize) = 0;
};

} // namespaec metadata
} // namespace sdk
} // namespace nx
