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
The example of incoming json 1:
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

/*static*/ std::optional<QSet<QString>> DeviceResponseJsonParser::parseEventTypes(
    const std::string& jsonReply)
{
/*
The example of incoming json (the beginning):
{
    "AlarmInput": {
        "1": false
    },
    "AlarmOutput": {
        "1": false
    },
    "ChannelEvent": [
        {
            "Channel": 0,
            "MotionDetection": false,
            "Tampering": false,
            "AudioDetection": false,
            "DefocusDetection": false,
...
*/
    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Root is not an object"));
        return std::nullopt;
    }

    const nx::kit::Json jsonChannelEvent = json["ChannelEvent"];
    if (!jsonChannelEvent.is_array())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. \"ChannelEvent\" field absent"));
        return std::nullopt;
    }

    QSet<QString> result;
    auto items = jsonChannelEvent[0].object_items();
    for (const auto& [key, value]: items)
    {
        if (key != "Channel") //< Channel is a special field, not an EventType
            result.insert(QString::fromStdString(key));
    }

    return result;
}

/*static*/ std::optional<FrameSize> DeviceResponseJsonParser::parseFrameSize(
    const std::string& jsonReply)
{
/*
The example of incoming json (the beginning):
{
    "VideoProfiles": [
        {
            "Channel": 0,
            "Profiles": [
                {
                    "Profile": 1,
                    "Name": "MJPEG",
                    "EncodingType": "MJPEG",
                    "RTPMulticastEnable": false,
                    "RTPMulticastAddress": "",
                    "RTPMulticastPort": 0,
                    "RTPMulticastTTL": 5,
                    "Resolution": "3840x2160",
                    "FrameRate": 1,
                    "CompressionLevel": 10,
                    "Bitrate": 6144,
...
*/
    std::string error;
    nx::kit::Json json = nx::kit::Json::parse(jsonReply, error);
    if (!json.is_object())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Root is not an object"));
        return std::nullopt;
    }

    const std::vector<nx::kit::Json>& videoProfiles = json["VideoProfiles"].array_items();
    if (videoProfiles.empty())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. \"VideoProfiles\" array absent"));
        return std::nullopt;
    }

    const std::vector<nx::kit::Json>& profiles = videoProfiles[0]["Profiles"].array_items();
    if (videoProfiles.empty())
    {
        NX_DEBUG(NX_SCOPE_TAG, NX_FMT("JSON parsing error. Channel 0 \"Profiles\" array absent"));
        return std::nullopt;
    }

    FrameSize maxFrameSize;
    for (const nx::kit::Json& profile: profiles)
    {
        std::string resolution = profile["Resolution"].string_value();
        std::istringstream stream(resolution);
        FrameSize profileFrameSize;
        char x;
        stream >> profileFrameSize.rawWidth >> x >> profileFrameSize.rawHeight;
        bool bad = !stream;

        if (!bad && x == 'x' && maxFrameSize < profileFrameSize)
            maxFrameSize = profileFrameSize;

    }
    return maxFrameSize;
}

/*static*/ std::optional<std::string> DeviceResponseJsonParser::parseFirmwareVersion(
    const std::string& jsonReply)
{
/*
The example of incoming json:
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
// Extract functions.
//-------------------------------------------------------------------------------------------------

/*static*/ nx::kit::Json DeviceResponseJsonParser::extractChannelInfo(
    const std::string& cameraReply, const char* eventName, int channelNumber)
{
    /*
    The example of incoming json:
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

    for (const auto& channel : jsonChannels.array_items())
    {
        if (const auto& j = channel["Channel"]; j.is_number() && j.int_value() == channelNumber)
            return channel;
    }
    return {};
}

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

    for (const nx::kit::Json& objectSize : objectSizeList.array_items())
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

    for (const nx::kit::Json& Line : Lines.array_items())
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

    for (const nx::kit::Json& Line : Lines.array_items())
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

    for (const nx::kit::Json& Line : Lines.array_items())
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

    for (const nx::kit::Json& Area : Areas.array_items())
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

} // namespace nx::vms_server_plugins::analytics::hanwha
