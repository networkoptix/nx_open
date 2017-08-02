#pragma once

#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractMediaFrame interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::GUID IID_MediaFrame
    = {{0x13, 0x85, 0x3c, 0xd6, 0x13, 0x7e, 0x4d, 0x8b, 0x9b, 0x8e, 0x63, 0xf1, 0x5f, 0x93, 0x2a, 0xc1}};

/**
 * @brief The AbstractMediaFrame class is an interface for decoded media frame.
 */
class AbstractMediaFrame: public AbstractDataPacket
{
public:

    /**
     * @return decoded data plane count.
     */
    virtual int planeCount() const = 0;

    /**
     * @return array of pointers to decoded data.
     * Size of this array is equal to the value return by planeCount call.
     */
    virtual const char** data() const = 0;

    /**
     * @return array of data plane sizes (in bytes).
     * Size of this array is equal to the value returned by planeCount call.
     */
    virtual const int* planeSize() const = 0;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
