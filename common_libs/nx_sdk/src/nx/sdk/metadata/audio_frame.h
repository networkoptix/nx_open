#pragma once

#include <nx/sdk/metadata/media_frame.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AudioFrame interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_AudioFrame =
    {{0x02,0x98,0x8d,0xda,0x71,0x55,0x49,0x85,0xb9,0x7e,0xbd,0x8f,0xcb,0x43,0x26,0x66}};

/**
 * Interface for the packet containing uncompressed audio data.
 */
class AudioFrame: public MediaFrame
{
public:
    /**
     * @return Duration of the audio frame in microseconds. Negative value in case of an error.
     */
    virtual int durationUsec() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
