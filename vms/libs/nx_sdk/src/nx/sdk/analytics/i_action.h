// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <nx/sdk/interface.h>

#include <nx/sdk/uuid.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/analytics/i_object_track_info.h>

namespace nx::sdk::analytics {

/**
 * Data supplied to IEngine::executeAction().
 */
class IAction: public Interface<IAction>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IAction"); }

    /** Id of the action being triggered. */
    virtual const char* actionId() const = 0;

    /** Called by objectTrackId() */
    protected: virtual void getObjectTrackId(Uuid* outValue) const = 0;
    /** Id of an object track for which the action has been triggered. */
    public: Uuid objectTrackId() const { Uuid value; getObjectTrackId(&value); return value; }

    /** Called by deviceId() */
    protected: virtual void getDeviceId(Uuid* outValue) const = 0;
    /** Id of a device from which the action has been triggered. */
    public: Uuid deviceId() const { Uuid value; getDeviceId(&value); return value; }

    /** Called by objectTrackInfo() */
    protected: virtual IObjectTrackInfo* getObjectTrackInfo() const = 0;
    /** Info about an object track this action has been triggered for. */
    public: Ptr<IObjectTrackInfo> objectTrackInfo() const { return Ptr(getObjectTrackInfo()); }

    /** Timestamp of a video frame from which the action has been triggered. */
    virtual int64_t timestampUs() const = 0;

    /** Called by params() */
    protected: virtual const IStringMap* getParams() const = 0;
    /**
     * If the Engine manifest defines params for this action type, contains the array of their
     * values after they are filled by the user via Client form. Otherwise, null.
     */
    public: Ptr<const IStringMap> params() const { return Ptr(getParams()); }

    /**
     * Data returned to the Server after the Action has been executed. Only one of the strings can
     * be set non-empty.
     */
    struct Result
    {
        /** If neither null nor empty, the Client will open this URL in an embedded browser. */
        Ptr<IString> actionUrl;

        /** If neither null nor empty, the Client will show this text to the user. */
        Ptr<IString> messageToUser;
    };
};
using IAction0 = IAction;

} // namespace nx::sdk::analytics
