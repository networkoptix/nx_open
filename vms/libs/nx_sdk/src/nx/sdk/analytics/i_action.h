// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <nx/sdk/interface.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/analytics/i_object_track_info.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Data supplied to IEngine::executeAction().
 */
class IAction: public Interface<IAction>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IAction"); }

    /** Id of the action being triggered. */
    virtual const char* actionId() const = 0;

    /** Id of an object track for which the action has been triggered. */
    protected: virtual void getObjectTrackId(Uuid* outValue) const = 0;
    public: Uuid objectTrackId() const { Uuid value; getObjectTrackId(&value); return value; }

    /** Id of a device from which the action has been triggered. */
    protected: virtual void getDeviceId(Uuid* outValue) const = 0;
    public: Uuid deviceId() const { Uuid value; getDeviceId(&value); return value; }

    /** Info about an object track this action has been triggered for. */
    protected: virtual IObjectTrackInfo* getObjectTrackInfo() const = 0;
    public: Ptr<IObjectTrackInfo> objectTrackInfo() const { return toPtr(getObjectTrackInfo()); }

    /** Timestamp of a video frame from which the action has been triggered. */
    virtual int64_t timestampUs() const = 0;

    /**
     * If the Engine manifest defines params for this action type, contains the array of their
     * values after they are filled by the user via Client form. Otherwise, null.
     */
    protected: virtual const IStringMap* getParams() const = 0;
    public: Ptr<const IStringMap> params() const { return toPtr(getParams()); }

    struct Result
    {
        /** If neither null nor empty, Client will open this URL in an embedded browser. */
        Ptr<IString> actionUrl;

        /** If neither null nor empty, Client will show this text to the user. */
        Ptr<IString> messageToUser;
    };
};

} // namespace analytics
} // namespace sdk
} // namespace nx
