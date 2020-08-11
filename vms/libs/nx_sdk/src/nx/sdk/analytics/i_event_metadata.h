// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

class IEventMetadata0: public Interface<IEventMetadata0, IMetadata>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadata"); }

    /**
     * @return Caption of the event, in UTF-8.
     */
    virtual const char* caption() const = 0;

    /**
     * @return Description of the event, in UTF-8.
     */
    virtual const char* description() const = 0;

    /**
     * @return Whether the event is in active state.
     */
    virtual bool isActive() const = 0;
};

class IEventMetadata: public Interface<IEventMetadata, IEventMetadata0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadata1"); }

    /** Called by trackId() */
    protected: virtual void getTrackId(Uuid* outValue) const = 0;
    /**
     * @return Id of the object track. Object track is a sequence of object detections from its
     *     first appearance on the scene till its disappearance. The same object can have multiple
     *     tracks (e.g. the same person entered and exited a room several times).
     */
    public: Uuid trackId() const { Uuid value; getTrackId(&value); return value; }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
