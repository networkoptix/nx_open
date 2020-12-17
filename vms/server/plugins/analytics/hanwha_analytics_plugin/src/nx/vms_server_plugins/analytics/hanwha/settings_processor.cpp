#include "settings_processor.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include <nx/network/http/http_client.h>

#include <plugins/resource/hanwha/hanwha_request_helper.h>

#include <algorithm>

#include "device_response_json_parser.h"

namespace nx::vms_server_plugins::analytics::hanwha {

namespace {

std::unique_ptr<nx::network::http::HttpClient> createHttpClient(
    const DeviceAccessInfo& deviceAccessInfo)
{
    auto result = std::make_unique<nx::network::http::HttpClient>();
    result->setUserName(deviceAccessInfo.user);
    result->setUserPassword(deviceAccessInfo.password);
    result->addAdditionalHeader("Accept", "application/json");
    return result;
}

}

//-------------------------------------------------------------------------------------------------

/**
 * Synchronously send reading request to the agent's device.
 * Typical parameter tuples for `domain/submenu/action`:
 *     eventstatus/eventstatus/check
 *     eventsources/tamperingdetection/view
 *     media/videoprofile/view
 * /return the answer from the device or empty string, if no answer received.
*/
std::string BasicSettingsProcessor::makeReadingRequestToDeviceSync(
    const char* domain, const char* submenu, const char* action, bool useChannel /*= true*/) const
{
    std::string result;
    nx::utils::Url command(m_deviceAccessInfo.url);
    constexpr const char* kPathPattern = "/stw-cgi/%1.cgi";
    constexpr const char* kQuerySubmenuActionPattern = "msubmenu=%1&action=%2";
    constexpr const char* kQueryChannelPattern = "&Channel=%1";
    QString query = QString::fromStdString(kQuerySubmenuActionPattern)
        .arg(submenu).arg(action);
    if (useChannel)
        query += QString::fromStdString(kQueryChannelPattern).arg(m_cameraChannelNumber);
    command.setPath(QString::fromStdString(kPathPattern).arg(domain));
    command.setQuery(query);
    command = m_valueTransformer->transformUrl(command); //< Changes command if NVR.

    std::unique_ptr<nx::network::http::HttpClient> settingsHttpClient =
        createHttpClient(m_deviceAccessInfo);

    const bool isSent = settingsHttpClient->doGet(command);
    if (!isSent)
        return result;

    auto* response = settingsHttpClient->response();
    const bool isOk = (response->statusLine.statusCode == 200);
    if (!isOk)
    {
        NX_DEBUG(this, NX_FMT("Reading request failed. Request = %1, Status = %2",
            command.toString(), response->statusLine.toString()));
    }

    auto messageBodyOptional = settingsHttpClient->fetchEntireMessageBody();
    if (messageBodyOptional.has_value())
    {
        result = messageBodyOptional->toStdString();
    }
    else
    {
        NX_DEBUG(this, NX_FMT("Reading request with empty response body. Request = %1",
            command.toString()));
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

/**
 * Synchronously send the request that should change analytics settings on the agent's device.
 * \return empty string, if the request is successful
 *     string with error message, if not
 */
std::string BasicSettingsProcessor::makeWritingRequestToDeviceSync(const std::string& query) const
{
    nx::utils::Url command(m_deviceAccessInfo.url);
    constexpr const char* kEventPath = "/stw-cgi/eventsources.cgi";
    command.setPath(kEventPath);
    command.setQuery(QString::fromStdString(query));
    command = m_valueTransformer->transformUrl(command);

    std::unique_ptr<nx::network::http::HttpClient> settingsHttpClient =
        createHttpClient(m_deviceAccessInfo);

    const bool isSent = settingsHttpClient->doGet(command);
    if (!isSent)
        return "Failed to send command to camera";

    auto* response = settingsHttpClient->response();
    if (response->statusLine.statusCode != 200)
        return response->statusLine.toString().toStdString();

    /* The Device may return 200 OK, and contain an error description in the body:
    {
        "Response": "Fail",
        "Error" :
         {
            "Code": 600,
            "Details" : "Submenu Not Found"
        }
    }
    */
    auto messageBodyOptional = settingsHttpClient->fetchEntireMessageBody();
    if (messageBodyOptional.has_value())
    {
        std::string body = messageBodyOptional->toStdString();
        auto errorMessage = DeviceResponseJsonParser::parseError(body);
        if (!errorMessage)
            return {};

        if (errorMessage->empty())
            return "Device answered with undescribed error";
        else
            return *errorMessage;
    }
    return {};
}

//-------------------------------------------------------------------------------------------------

std::string BasicSettingsProcessor::makeEventTypeReadingRequest(const char* eventTypeInternalName) const
{
    return makeReadingRequestToDeviceSync("eventsources", eventTypeInternalName, "view");
}

//-------------------------------------------------------------------------------------------------

std::string BasicSettingsProcessor::makeOrientationReadingRequest() const
{
    return makeReadingRequestToDeviceSync("image", "flip", "view");
}

//-------------------------------------------------------------------------------------------------

/**
 * Used for NVRs only. Get the event types, supported directly by the device, not by NVR channel.
 * For non-NVR devices the event types are taken from Hanwha shared context.
 */
std::string BasicSettingsProcessor::makeEventsStatusReadingRequest() const
{
    return makeReadingRequestToDeviceSync("eventstatus", "eventstatus", "check");
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void SettingsProcessor::updateAnalyticsModeOnDevice() const
{
    const int channelNumber = m_valueTransformer->transformChannelNumber(m_cameraChannelNumber);
    AnalyticsMode currentAnalyticsMode(
        m_settings.shockDetection.m_settingsCapabilities, m_roiResolution);
    std::string sunapiReply = makeEventTypeReadingRequest("videoanalysis2");
    SettingGroup::readFromDeviceReply(sunapiReply, &currentAnalyticsMode, channelNumber);

    AnalyticsMode desiredAnalyticsMode = (m_settings.IntelligentVideoIsActive())
        ? currentAnalyticsMode.addIntelligentVideoMode()
        : currentAnalyticsMode.removeIntelligentVideoMode();

    if (desiredAnalyticsMode != currentAnalyticsMode)
    {
        const std::string query =
            desiredAnalyticsMode.buildDeviceWritingQuery(m_cameraChannelNumber);
        const std::string error = makeWritingRequestToDeviceSync(query);
        if (!error.empty())
            NX_DEBUG(this, NX_FMT("Request failed. Url = %1", query));
    }

}

//-------------------------------------------------------------------------------------------------

std::string SettingsProcessor::loadFirmwareVersionFromDevice() const
{
    // request path = /stw-cgi/system.cgi?msubmenu=deviceinfo&action=view
    std::string jsonReply = makeReadingRequestToDeviceSync("system", "deviceinfo", "view",
        false /*useChanel*/);
    std::string result;
    auto firmwareVersion = DeviceResponseJsonParser::parseFirmwareVersion(jsonReply);
    if (firmwareVersion)
        result = *firmwareVersion;
    return result;
}

//-------------------------------------------------------------------------------------------------

void SettingsProcessor::loadAndHoldFrameRotationFromDevice()
{
    const int channelNumber = m_valueTransformer->transformChannelNumber(m_cameraChannelNumber);
    const std::string sunapiReply = makeOrientationReadingRequest();
    nx::kit::Json channelInfo = DeviceResponseJsonParser::extractChannelInfo(
        sunapiReply, "Flip", channelNumber);

    const std::optional<bool> isRotated =
        DeviceResponseJsonParser::parseFrameIsRotated(sunapiReply, channelNumber);

    m_roiResolution.isRotated = isRotated.has_value() && *isRotated;
}

//-------------------------------------------------------------------------------------------------

void SettingsProcessor::loadAndHoldExclusiveSettingsFromServer(const nx::sdk::IStringMap* sourceMap)
{
    const auto sender = [this](const std::string& request)
    {
        return this->makeWritingRequestToDeviceSync(request);
    };

    if (m_settings.analyticsCategories[videoAnalytics])
    {
        for (int i = 0; i < Settings::kMultiplicity; ++i)
            m_settings.ivaLines[i].readExclusiveFromServer(sourceMap);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
            m_settings.ivaAreas[i].readExclusiveFromServer(sourceMap);
    }
}

//-------------------------------------------------------------------------------------------------

void SettingsProcessor::loadAndHoldSettingsFromDevice()
{
    // Updates m_frameSize::isRotated, many all roi settings depend on it.
    loadAndHoldFrameRotationFromDevice();

    std::string sunapiReply;
    const int channelNumber = m_valueTransformer->transformChannelNumber(m_cameraChannelNumber);

    if (m_settings.analyticsCategories[shockDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("shockdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.shockDetection, channelNumber);
    }

    if (m_settings.analyticsCategories[tamperingDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("tamperingdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.tamperingDetection, channelNumber);
    }

    sunapiReply.clear();

    if (m_settings.analyticsCategories[videoAnalytics])
    {
        if (sunapiReply.empty())
            sunapiReply = makeEventTypeReadingRequest("videoanalysis2");

        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.ivaObjectSize, channelNumber);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::readFromDeviceReply(
                sunapiReply, &m_settings.ivaLines[i], channelNumber, i);
        }

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::readFromDeviceReply(
                sunapiReply, &m_settings.ivaAreas[i], channelNumber, i);
        }

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::readFromDeviceReply(
                sunapiReply, &m_settings.ivaExcludeAreas[i], channelNumber, i);
        }
    }

    if (m_settings.analyticsCategories[defocusDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("defocusdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.defocusDetection, channelNumber);
    }

    if (m_settings.analyticsCategories[fogDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("fogdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.fogDetection, channelNumber);
    }

    if (m_settings.analyticsCategories[objectDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("objectdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.objectDetectionGeneral, channelNumber);

        sunapiReply = makeEventTypeReadingRequest("metaimagetransfer");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.objectDetectionBestShot, channelNumber);
    }

    if (m_settings.analyticsCategories[audioDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("audiodetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.audioDetection, channelNumber);
    }

    if (m_settings.analyticsCategories[audioAnalytics])
    {
        sunapiReply = makeEventTypeReadingRequest("audioanalysis");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.soundClassification, channelNumber);
    }

    if (m_settings.analyticsCategories[faceMaskDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("maskdetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.faceMaskDetection, channelNumber);
    }

    if (m_settings.analyticsCategories[temperatureChangeDetection])
    {
        sunapiReply = makeEventTypeReadingRequest("boxtemperaturedetection");
        SettingGroup::readFromDeviceReply(
            sunapiReply, &m_settings.temperatureChangeDetectionToggle, channelNumber);

        for (int i = 0; i < Settings::kTemperatureMultiplicity; ++i)
        {
            SettingGroup::readFromDeviceReply(
                sunapiReply, &m_settings.temperatureChangeDetectionItems[i], channelNumber, i);
        }
    }
}

//-------------------------------------------------------------------------------------------------

void SettingsProcessor::transferAndHoldSettingsFromDeviceToServer(
    nx::sdk::SettingsResponse* response)
{
    loadAndHoldSettingsFromDevice();

    if (m_settings.analyticsCategories[shockDetection])
        m_settings.shockDetection.writeToServer(response);

    if (m_settings.analyticsCategories[tamperingDetection])
        m_settings.tamperingDetection.writeToServer(response);

    if (m_settings.analyticsCategories[defocusDetection])
        m_settings.defocusDetection.writeToServer(response);

#if 1
    // Fog detection is broken in Hanwha firmware <= 1.41.
    if (m_settings.analyticsCategories[fogDetection])
        m_settings.fogDetection.writeToServer(response);
#endif

    if (m_settings.analyticsCategories[videoAnalytics])
    {
        // AnalyticsMode (i.e DetectionType) is not stored on the server, we don't save it.

        m_settings.ivaObjectSize.writeToServer(response);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
            m_settings.ivaLines[i].writeToServer(response);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
            m_settings.ivaAreas[i].writeToServer(response);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
            m_settings.ivaExcludeAreas[i].writeToServer(response);
    }

    if (m_settings.analyticsCategories[objectDetection])
    {
        m_settings.objectDetectionGeneral.writeToServer(response);
        m_settings.objectDetectionBestShot.writeToServer(response);
    }

    if (m_settings.analyticsCategories[audioDetection])
        m_settings.audioDetection.writeToServer(response);

    if (m_settings.analyticsCategories[audioAnalytics])
        m_settings.soundClassification.writeToServer(response);

    if (m_settings.analyticsCategories[faceMaskDetection])
        m_settings.faceMaskDetection.writeToServer(response);

    if (m_settings.analyticsCategories[temperatureChangeDetection])
    {
        m_settings.temperatureChangeDetectionToggle.writeToServer(response);

        for (int i = 0; i < Settings::kTemperatureMultiplicity; ++i)
            m_settings.temperatureChangeDetectionItems[i].writeToServer(response);
    }
}

//-------------------------------------------------------------------------------------------------

void SettingsProcessor::transferAndHoldSettingsFromServerToDevice(
    nx::sdk::StringMap* errorMap,
    nx::sdk::StringMap* valueMap,
    const nx::sdk::IStringMap* sourceMap)
{
    const auto sender = [this](const std::string& request)
    {
        return this->makeWritingRequestToDeviceSync(request);
    };

    if (m_settings.analyticsCategories[motionDetection])
    {
#if 0
        // Hanwha analyticsMode detection selection is currently removed from the Clients interface
        // desired analytics mode if selected implicitly now.
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.analyticsMode, sender, m_cameraChannelNumber);
#endif
    }

    if (m_settings.analyticsCategories[shockDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.shockDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[tamperingDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.tamperingDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[defocusDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.defocusDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[fogDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.fogDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[videoAnalytics])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.ivaObjectSize, sender, m_cameraChannelNumber);

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
                m_settings.ivaLines[i], sender, m_cameraChannelNumber, i);
        }

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
                m_settings.ivaAreas[i], sender, m_cameraChannelNumber, i);
        }

        for (int i = 0; i < Settings::kMultiplicity; ++i)
        {
            SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
                m_settings.ivaExcludeAreas[i], sender, m_cameraChannelNumber, i);
        }

        updateAnalyticsModeOnDevice();
    }

    if (m_settings.analyticsCategories[objectDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.objectDetectionGeneral, sender, m_cameraChannelNumber);

        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.objectDetectionBestShot, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[audioDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.audioDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[audioAnalytics])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.soundClassification, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[faceMaskDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.faceMaskDetection, sender, m_cameraChannelNumber);
    }

    if (m_settings.analyticsCategories[temperatureChangeDetection])
    {
        SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
            m_settings.temperatureChangeDetectionToggle, sender, m_cameraChannelNumber);

        for (int i = 0; i < Settings::kTemperatureMultiplicity; ++i)
        {
            SettingGroup::transferFromServerToDevice(errorMap, valueMap, sourceMap,
                m_settings.temperatureChangeDetectionItems[i], sender, m_cameraChannelNumber, i);
        }
    }
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
