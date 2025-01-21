// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/interface.h>

#include "i_metadata.h"

namespace nx::sdk::analytics {

class IEventMetadata0: public Interface<IEventMetadata0, IMetadata0>
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

class IEventMetadata1: public Interface<IEventMetadata1, IEventMetadata0>
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

class IEventMetadata2: public Interface<IEventMetadata2, IEventMetadata1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadata2"); }

    /**
     * Optional property used to identify duplicates of state-dependent Events which are isActive.
     *
     * State-dependent (prolonged) Events allow for duplicate EventMetadata instances that denote
     * the beginning of the Event (with isActive() being true) - all such duplicates except the
     * first one are ignored by the Server. To be able to do so, the Server uses a key to identify
     * such duplicates, which consists of the Event's type id, track id (if specified) and this
     * property (if specified).
     */
    virtual const char* key() const = 0;
};

class IEventMetadata: public Interface<IEventMetadata, IEventMetadata2>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEventMetadata3"); }

    /** Called by boundingBox() */
    protected: virtual void getBoundingBox(Rect* outValue) const = 0;
    /**
     * @return Bounding box of the video frame fragment associated with the event (the video frame
     *     timestamp is assumed to be equal to the Event timestamp), or an invalid rectangle (e.g.
     *     a default-constructed one) if no image is associated with the Event.
     */
    public: Rect boundingBox() const { Rect value; getBoundingBox(&value); return value; }
};
using IEventMetadata3 = IEventMetadata;

} // namespace nx::sdk::analytics
