// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/analytics/i_metadata.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * A single object detected on the scene on a particular video frame, defined as a bounding box.
 */
class IObjectMetadata: public Interface<IObjectMetadata, IMetadata>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IObjectMetadata"); }

    /**
     * @return Id of the object track. Object track is a sequence of object detections from its
     *     first appearance on the scene till its disappearing. The same object can have multiple
     *     tracks (e.g. the same person entered and exited a room several times).
     */
    virtual Uuid trackId() const = 0;

    /**
     * @return Subclass of the object (e.g. vehicle type: truck, car, etc.).
     */
    virtual const char* subtype() const = 0;

    /**
     * @return Bounding box of an object detected in a video frame.
     */
    virtual Rect boundingBox() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
