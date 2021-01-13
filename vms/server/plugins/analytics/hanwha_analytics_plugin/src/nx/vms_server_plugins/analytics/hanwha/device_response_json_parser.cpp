#include "device_response_json_parser.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<std::string> DeviceResponseJsonParser::parseError(
    const std::string& jsonReply)
{
/*
The example of the incoming json 1:
{
    "Response": "Success"
}
The example of incoming json 2:
{
    "Response": "Fail",
    "Error": {
        "Code": 600,
        "Details": "Submenu Not Found"
    }
}
*/
    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Root is not an object"));
        return std::nullopt; //< not an error
    }

    if (json["Response"].string_value() == "Fail")
        return json["Error"]["Details"].string_value(); //< may be empty

    return std::nullopt; //< not an error
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<std::string> DeviceResponseJsonParser::parseFirmwareVersion(
    const std::string& jsonReply)
{
/*
The example of the incoming json:
{
    "Model": "PNV-A9081R",
    "SerialNumber": "ZNKZ70GM800002Y",
    "FirmwareVersion": "1.41.01_20200318_R342",
    "BuildDate": "2020.03.18",
    "WebURL": "http://www.hanwha-security.com/",
    "DeviceType": "NWC",
    "ConnectedMACAddress": "00:09:18:61:9F:28",
    "ISPVersion": "1.00_200302",
    "BootloaderVersion": "       ",
    "CGIVersion": "2.5.7",
    "ONVIFVersion": "18.6",
    "DeviceName": "Camera",
    "DeviceLocation": "Location",
    "DeviceDescription": "Description",
    "Memo": "Memo",
    "Language": "English",
    "PasswordStrength": "Strong",
    "OpenSDKVersion": "3.60_191204",
    "FirmwareGroup": ""
}
*/
    std::string error;
    const nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Root is not an object"));
        return std::nullopt;
    }

    const nx::kit::Json jsonFirmwareVersion = json["FirmwareVersion"];
    if (!jsonFirmwareVersion.is_string())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. \"FirmwareVersion\" field absent"));
        return std::nullopt;
    }
    return json["FirmwareVersion"].string_value();
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<bool> DeviceResponseJsonParser::parseFrameIsRotated(
    const std::string& jsonReply, int chanelNumber)
{
/*
The example of the incoming json:
{
    "Flip": [
        {
            "Channel": 0,
            "HorizontalFlipEnable": false,
            "VerticalFlipEnable": false,
            "Rotate": "270"
        }
    ]
}
*/
    nx::kit::Json channelInfo = extractChannelInfo(jsonReply, "Flip", chanelNumber);

    if (!channelInfo.is_object())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Root is not an object"));
        return std::nullopt;
    }

    const nx::kit::Json jsonAngle = channelInfo["Rotate"];
    if (!jsonAngle.is_string())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. \"Rotate\" field absent"));
        return std::nullopt;
    }

    const std::string rotationAngleAsString = jsonAngle.string_value();

    int rotationAngle = 0;
    try
    {
        rotationAngle = stoi(rotationAngleAsString); //< may throw
        const int kExpectedRotateValues[] = { 0, 90, 270 };
        if (std::find(std::cbegin(kExpectedRotateValues),
            std::cend(kExpectedRotateValues),
            rotationAngle) == std::cend(kExpectedRotateValues))
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "Unexpected rotation angle %1 returned, 0 value will be used instead",
                rotationAngle);
            return std::nullopt;
        }
    }
    catch (const std::logic_error&)
    {
        // `stoi` may throw exception `out_of_range` or `invalid_argument`, that are
        // descendants of `logic_error`
        NX_DEBUG(NX_SCOPE_TAG, "Rotation angle read in not an integer");
        return std::nullopt;
    }
    return (rotationAngle != 0);
}

//-------------------------------------------------------------------------------------------------
// Extract functions.
//-------------------------------------------------------------------------------------------------

/*static*/ nx::kit::Json DeviceResponseJsonParser::extractChannelInfo(
    const std::string& cameraReply, const char* eventName, int channelNumber)
{
    /*
    The example of the incoming json:
    {
        "ShockDetection": [
            {
                "Channel": 0,
                "Enable": false,
                "Sensitivity": 41,
                "ThresholdLevel": 31
            }
        ]
    }
    */
    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(cameraReply, err);
    if (!json.is_object())
        return {};

    const nx::kit::Json& jsonChannels = json[eventName];
    if (!jsonChannels.is_array())
        return {};

    for (const auto& channel: jsonChannels.array_items())
    {
        if (const auto& j = channel["Channel"]; j.is_number() && j.int_value() == channelNumber)
            return channel;
    }
    return {};
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<bool> DeviceResponseJsonParser::extractBoxTemperatureDetectionToggle(
    const nx::kit::Json& channelInfo, int chanelNumber)
{
    const nx::kit::Json jsonEnable = channelInfo["Enable"];
    if (!jsonEnable.is_bool())
    {
        NX_DEBUG(NX_SCOPE_TAG, "JSON parsing error. \"Enable\" field absent");
        return std::nullopt;
    }
    return jsonEnable.bool_value();
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<bool> DeviceResponseJsonParser::extractTemperatureChangeDetectionToggle(
    const nx::kit::Json& channelInfo, int chanelNumber)
{
    const nx::kit::Json jsonEnable = channelInfo["Enable"];
    if (!jsonEnable.is_bool())
    {
        NX_DEBUG(NX_SCOPE_TAG, "JSON parsing error. \"Enable\" field absent");
        return std::nullopt;
    }
    return jsonEnable.bool_value();
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about min and max object size (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractObjectSizeInfo(
    const nx::kit::Json& channelInfo, const std::string& detectionTypeValue)
{
    nx::kit::Json result;
    const nx::kit::Json& objectSizeList = channelInfo["ObjectSizeByDetectionTypes"];
    if (!objectSizeList.is_array())
        return result;

    for (const nx::kit::Json& objectSize: objectSizeList.array_items())
    {
        const nx::kit::Json& detectionTypeParameter = objectSize["DetectionType"];
        if (detectionTypeParameter.string_value() == detectionTypeValue)
        {
            result = objectSize;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

// TODO: Unite extractMdRoiInfo, extractIvaLineInfo, extractIvaRoiInfo, extractOdRoiInfo into one function

/**
 * Extract information about analyticsMode detection ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractMdRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["ROIs"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["ROI"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about IVA line (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractIvaLineInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["Lines"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["Line"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about IVA ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractIvaRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["DefinedAreas"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["DefinedArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about object detection ROI (as a json object) of a desired type from
 * the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractOdRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Areas = channelInfo["ExcludeAreas"];
    if (!Areas.is_array())
        return result;

    for (const nx::kit::Json& Area: Areas.array_items())
    {
        const nx::kit::Json& roiIndex = Area["ExcludeArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Area;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about face mask detection ROI (exclude area) (as a json object)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractMaskRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& Lines = channelInfo["ExcludeAreas"];
    if (!Lines.is_array())
        return result;

    for (const nx::kit::Json& Line: Lines.array_items())
    {
        const nx::kit::Json& roiIndex = Line["ExcludeArea"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = Line;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about box temperature detection ROI (as a json object) of a desired
 * type from the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractBoxTemperatureDetectionRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& rois = channelInfo["ROIs"];
    if (!rois.is_array())
        return result;

    for (const nx::kit::Json& roi: rois.array_items())
    {
        const nx::kit::Json& roiIndex = roi["ROI"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = roi;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Extract information about temperature change detection ROI (as a json object) of a desired
 * type from the json object (that corresponds to some event and channel)
 */
/*static*/ nx::kit::Json DeviceResponseJsonParser::extractTemperatureChangeDetectionRoiInfo(
    nx::kit::Json channelInfo, int sunapiIndex)
{
    nx::kit::Json result;
    const nx::kit::Json& rois = channelInfo["TemperatureChange"];
    if (!rois.is_array())
        return result;

    for (const nx::kit::Json& roi: rois.array_items())
    {
        const nx::kit::Json& roiIndex = roi["ROI"];
        if (roiIndex.is_number() && roiIndex.int_value() == sunapiIndex)
        {
            result = roi;
            return result;
        }
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
