#pragma once

#include <plugins/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractEventMetadataPacket interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::GUID IID_EventMetadataPacket
    = {{0x20, 0xfc, 0xa8, 0x08, 0x17, 0x6b, 0x48, 0xa6, 0x92, 0xfd, 0xba, 0xb5, 0x9d, 0x37, 0xd7, 0xc0}};

/**
 * @brief The AbstractEventMetadataPacket class is an interface for metadata packet
 * containig information about some event.
 */
class AbstractEventMetadataPacket: public AbstractMetadataPacket
{
    /**
     * @return Null terminated ASCII string determinig the type of the event.
     */
    virtual NX_ASCII const char* typeId() const = 0;

    /**
     * @return Null terminated UTF8 string containig the caption of the event.
     */
    virtual NX_LOCALE_DEPENDENT const char* caption() const = 0;

    /**
     * @return Null terminated UTF8 string containig the description of the event.
     */
    virtual NX_LOCALE_DEPENDENT const char* description() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
