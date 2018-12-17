#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/utils/std/optional.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

/**
 * Stateless namespace-like class, that contains functions for camera messages parsing.
 * All functions are static.
 */
class Parser
{
public:
    /**
     * Extract the list of supported events of a camera from the message (the list of camera
     * supported  events is a subset of the list of engine supported events).
     * @param[in] content The content of the message with analytic events supported by a camera.
     * @return The list of internal names of supported events.
     */
    static std::vector<QString> parseSupportedEventsMessage(const QByteArray& content);

    /**
     * Extract the information about the event occurred from notification message and create
     * the corresponding Event object.
     * @param[in] content The content of the notification message.
     * @param[in] EngineManifest Manifest, that contains engine's possible event types and their
     * properties.
     * @return std::nullopt, if the message is incomplete or invalid, extracted Event object, if
     * message is complete and valid.
     */
    static std::optional<Event> parseEventMessage(const QByteArray& content,
        const EngineManifest& engineManifest);

    /**
     * Check if the event's typeId is "Heartbeat".
     * @note Heartbeat is a special event type. It is not included in engine's of camera's
     * supported events.
     * @param[in] event Event to check.
     * @return
     */
    static bool isHartbeatEvent(const Event& event);
};

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
