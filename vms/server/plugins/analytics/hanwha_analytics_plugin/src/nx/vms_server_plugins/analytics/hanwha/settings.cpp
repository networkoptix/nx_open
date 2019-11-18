#include "settings.h"

#include <charconv>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

ShockDetection::ShockDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;
    enabled = (params[0] == "True");
    char* end = nullptr;
    // Standard locale-independent conversion designed for JSON.
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    initialized = true;
}

bool ShockDetection::operator==(const ShockDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

Motion::Motion(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    detectionType = params[0];
    initialized = true;
}

bool Motion::operator==(const Motion& rhs) const
{
    return detectionType == rhs.detectionType
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

IncludeArea::IncludeArea(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    char* end = nullptr;
    // Standard locale-independent conversion designed for JSON.
    points = sunapiStringToSunapiPoints(params[0]);
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    initialized = true;
}

bool IncludeArea::operator==(const IncludeArea& rhs) const
{
    return points == rhs.points
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

ExcludeArea::ExcludeArea(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    char* end = nullptr;
    points = sunapiStringToSunapiPoints(params[0]);
    initialized = true;
}

//-------------------------------------------------------------------------------------------------

TamperingDetection::TamperingDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    enabled = (params[0] == "True");
    char* end = nullptr;
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    exceptDarkImages = (params[4] == "True");
    initialized = true;
}

bool TamperingDetection::operator==(const TamperingDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && exceptDarkImages == rhs.exceptDarkImages
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

DefocusDetection::DefocusDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    enabled = (params[0] == "True");
    char* end = nullptr;
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    std::from_chars(&params[2].front(), &params[2].back() + 1, sensitivityLevel);
    std::from_chars(&params[3].front(), &params[3].back() + 1, minimumDuration);
    initialized = true;
}

bool DefocusDetection::operator==(const DefocusDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel
        && sensitivityLevel == rhs.sensitivityLevel
        && minimumDuration == rhs.minimumDuration
        && initialized == rhs.initialized;
}

//-------------------------------------------------------------------------------------------------

AudioDetection::AudioDetection(const std::vector<std::string>& params)
{
    if (std::size(params) != std::size(kParams))
        return;

    enabled = (params[0] == "True");
    char* end = nullptr;
    std::from_chars(&params[1].front(), &params[1].back() + 1, thresholdLevel);
    initialized = true;
}

bool AudioDetection::operator==(const AudioDetection& rhs) const
{
    return enabled == rhs.enabled
        && thresholdLevel == rhs.thresholdLevel;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
