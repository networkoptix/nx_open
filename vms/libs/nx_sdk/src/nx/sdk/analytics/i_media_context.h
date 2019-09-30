// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Codec context for encoding/decoding.
 */
class IMediaContext: public Interface<IMediaContext>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMediaContext"); }

    /**
     * @return pointer to codec specific blob of extradata or nullptr (if no extradata needed).
     */
    virtual const char* extradata() const;

    /**
     * @return size of blob returned by extradata call (in bytes).
     */
    virtual int extradataSize() const;
};

} // namespace analytics
} // namesapce sdk
} // namespace nx
