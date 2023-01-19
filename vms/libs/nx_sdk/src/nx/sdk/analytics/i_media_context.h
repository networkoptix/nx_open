// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Codec context for encoding/decoding.
 */
class IMediaContext: public Interface<IMediaContext>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMediaContext"); }

    /**
     * @return Pointer to the codec-specific blob of extradata, or null if no extradata is
     *     available.
     */
    virtual const char* extradata() const = 0;

    /**
     * @return Size of the data returned by the extradata(), in bytes.
     */
    virtual int extradataSize() const = 0;
};
using IMediaContext0 = IMediaContext;

} // namespace nx::sdk::analytics
