#ifndef NX_PLUGIN_CONTAINER_API_H
#define NX_PLUGIN_CONTAINER_API_H

#include <stdint.h>

#include "plugin_api.h"


namespace nxpl
{
    // {A6313921-C469-4E6C-9E5E-8851B8E00941}
    static const NX_GUID IID_PluginContainer = { { 0xa6, 0x31, 0x39, 0x21, 0xc4, 0x69, 0x4e, 0x6c, 0x9e, 0x5e, 0x88, 0x51, 0xb8, 0xe0, 0x9, 0x41 } };

    // {8118AE76-37A1-4849-83E2-54C11EBF5A22}
    static const NX_GUID IID_TimeProvider = { { 0x81, 0x18, 0xae, 0x76, 0x37, 0xa1, 0x48, 0x49, 0x83, 0xe2, 0x54, 0xc1, 0x1e, 0xbf, 0x5a, 0x22 } };

    //!Provides access to synchronized time
    /*!
        Time is synchronized with internet time server or with mediaserver, selected by user
    */
    class TimeProvider
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~TimeProvider() {}

        //!Synchronized time in milliseconds from 1970-01-01T00:00 UTC
        virtual uint64_t millisSinceEpoch() const = 0;
    };
}

#endif  //NX_PLUGIN_CONTAINER_API_H
