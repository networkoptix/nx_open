#include "common_tools.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static float kAspectRatioEpsilon = 0.04f;

float aspectRatio(const QSize& resolution)
{
    if (!resolution.isEmpty())
        return resolution.width() / (float)resolution.height();
    else
        return 1.0;
};

QSize nearestResolution(
    const QSize& requestedResolution,
    const std::vector<QSize>& resolutionList,
    float requestedAspect)
{
    if (resolutionList.empty())
        return QSize();

    int bestIndex = -1;
    float bestMatchCoeff = std::numeric_limits<float>::max();
    const float requestedSquare = requestedResolution.width() * requestedResolution.height();

    for (int i = 0; i < resolutionList.size(); ++i)
    {
        const auto currentAspect = aspectRatio(resolutionList[i]);
        const bool isAspectOk = requestedAspect == 0
            || std::abs(currentAspect - requestedAspect) < kAspectRatioEpsilon;

        if (!isAspectOk)
            continue;

        const float currentResolutionArea = resolutionList[i].width() * resolutionList[i].height();
        const float matchCoeff = std::max(requestedSquare, currentResolutionArea)
            / std::min(requestedSquare, currentResolutionArea);

        if (matchCoeff < bestMatchCoeff)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }
    return bestIndex >= 0 ? resolutionList[bestIndex] : QSize();
};

} // namespace

QSize secondaryStreamResolution(
    const QSize& primaryStreamResolution,
    const std::vector<QSize>& resolutionList)
{
    auto resolution = nearestResolution(
        kDefaultSecondaryResolution,
        resolutionList,
        aspectRatio(primaryStreamResolution));

    if (resolution.isEmpty())
    {
        resolution = nearestResolution(
            kDefaultSecondaryResolution,
            resolutionList,
            0.0);
    }

    return resolution;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
