#pragma once

#include <optional>
#include <string>

#include <QtCore/QSet>
#include <QtCore/QString>

#include "setting_primitives.h"

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

class DeviceResponseJsonParser
{
public:

    /**
     * Parse the reply to setting request.
     * \return error message, if jsonReply indicates an error and contains an error message
     *     empty string, if jsonReply indicates an error and contains no error message
     *     std::nullopt, if jsonReply does not indicates an error
     */
    static std::optional<std::string> parseError(const std::string& jsonReply);

    /**
     * Parse the reply to check eventstatus BYPASS request to get the list of event types,
     * supported by the device, connected to the current channel of NVR.
     */
    static std::optional<QSet<QString>> parseEventTypes(const std::string& jsonReply);

    /**
     * Extract frameSize from json string. If fail, returns std::nullopt.
     */
    static std::optional<FrameSize> parseFrameSize(const std::string& jsonReply);

    /** Extract FirmwareVersion from json string. If fail, returns std::nullopt.*/
    static std::optional<std::string> parseFirmwareVersion(const std::string& jsonReply);

    // Extracting functions:

    /**
     * Parse the sunapi reply from camera and extract (as a json object) information about the desired
     * event on the desired channel. If nothing found, empty json object returned.
     */
    static nx::kit::Json extractChannelInfo(
        const std::string& cameraReply, const char* eventName, int channelNumber);

    /**
     * Extract information about min and max object size (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json extractObjectSizeInfo(const nx::kit::Json& channelInfo,
        const std::string& detectionTypeValue);

    /**
     * Extract information about analyticsMode detection ROI (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json extractMdRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about IVA line (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json extractIvaLineInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about IVA ROI (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json  DeviceResponseJsonParser::extractIvaRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about object detection ROI (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json  DeviceResponseJsonParser::extractOdRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
