#pragma once

#include <stdint.h>

#include <plugins/plugin_api.h>

namespace nxpl {

static const NX_GUID IID_TimeProvider = //< {8118AE76-37A1-4849-83E2-54C11EBF5A22}
    {{0x81,0x18,0xae,0x76,0x37,0xa1,0x48,0x49,0x83,0xe2,0x54,0xc1,0x1e,0xbf,0x5a,0x22}};

/**
 * Provides access to synchronized time. Time is synchronized with an internet time server or with
 * mediaserver, selected by the user.
 */
class TimeProvider: public nxpl::PluginInterface
{
public:
    virtual ~TimeProvider() {}

    /** Synchronized time in milliseconds from 1970-01-01T00:00 UTC. */
    virtual uint64_t millisSinceEpoch() const = 0;
};

} // namespace
