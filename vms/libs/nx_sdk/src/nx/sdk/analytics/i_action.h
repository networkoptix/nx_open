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
 * Object supplied to IEngine::executeAction().
 */
class IAction: public Interface<IAction>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IAction"); }

    /** Id of the action being triggered. */
    virtual const char* actionId() const = 0;

    /** Id of a metadata object for which the action has been triggered. */
    virtual Uuid objectId() const = 0;

    /** Id of a device from which the action has been triggered. */
    virtual Uuid deviceId() const = 0;

    /** Info about an object track this action has been triggered from. */
    virtual IObjectTrackInfo* objectTrackInfo() const = 0;

    /** Timestamp of a video frame from which the action has been triggered. */
    virtual int64_t timestampUs() const = 0;

    /**
     * If the Engine manifest defines params for this action type, contains the array of their
     * values after they are filled by the user via Client form. Otherwise, null.
     */
    virtual const nx::sdk::IStringMap* params() const = 0;

    /**
     * Report action result back to Server. If the action is decided not to have any result, this
     * method can be either called with nulls or not called at all.
     * @param actionUrl If not null, Client will open this URL in an embedded browser.
     * @param messageToUser If not null, Client will show this text to the user.
     */
    virtual void handleResult(
        const char* actionUrl,
        const char* messageToUser) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
