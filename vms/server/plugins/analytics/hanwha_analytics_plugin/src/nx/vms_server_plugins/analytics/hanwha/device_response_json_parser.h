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

    /** Extract FirmwareVersion from json string. If fail, returns std::nullopt.*/
    static std::optional<std::string> parseFirmwareVersion(const std::string& jsonReply);

    /** Extract if video is rotated (to portrait) mode from json string. If fail, returns std::nullopt.*/
    static std::optional<bool> parseFrameIsRotated(const std::string& jsonReply, int chanelNumber);

    // Extracting functions:

    /**
     * Parse the sunapi reply from camera and extract (as a json object) information about the desired
     * event on the desired channel. If nothing found, empty json object returned.
     */
    static nx::kit::Json extractChannelInfo(
        const std::string& cameraReply, const char* eventName, int channelNumber);

    /** Extract if BoxTemperatureDetection is enabled If fail, returns std::nullopt.*/
    static std::optional<bool> extractBoxTemperatureDetectionToggle(
        const nx::kit::Json& channelInfo, int chanelNumber);

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
    static nx::kit::Json extractIvaRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about object detection ROI (as a json object) of a desired type from
     * the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json extractOdRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about face mask detection ROI (exclude area) (as a json object);
     */
    static nx::kit::Json extractMaskRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

    /**
     * Extract information about box temperature detection ROI (as a json object) of a desired
     * type from the json object (that corresponds to some event and channel)
     */
    static nx::kit::Json extractTemperatureRoiInfo(nx::kit::Json channelInfo, int sunapiIndex);

};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
