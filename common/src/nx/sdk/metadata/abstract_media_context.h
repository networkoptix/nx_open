#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements AbstractMediaContext interface
 * should properly handle this GUID in its queryInterface method.
 */
static const nxpl::NX_GUID IID_MediaContext
    = {{0x9b, 0x1a, 0xca, 0xa7, 0x29, 0x48, 0x4b, 0xe2, 0xb0, 0xed, 0x69, 0xfb, 0x57, 0x3b, 0x66, 0x56}};

/**
 * @brief The AbstractMediaContext class is an interface for decoding / encoding context.
 */
class AbstractMediaContext: public nxpl::PluginInterface
{
public:
    /**
     * @return pointer to codec specific blob of extradata or nullptr (if no extradata needed).
     */
    virtual const char* extradata() const;

    /**
     * @return size of blob returned by extradata call (in bytes).
     */
    virtual int extradataSize() const;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
