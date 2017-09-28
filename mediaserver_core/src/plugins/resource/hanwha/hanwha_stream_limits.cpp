#include "hanwha_stream_limits.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const std::pair<int, int> kInvalidIntegerLimits = {-1, -1};

} // namespace

const std::set<QString> kHanwhaStreamLimitParameters = {
    lit("Resolution"),
    lit("FrameRate"),
    lit("Bitrate"),
    lit("EncodingType"),
    lit("H264.BitrateControlType"),
    lit("H264.GOVLength"),
    lit("H264.PriorityType"),
    lit("H264.Profile"),
    lit("H264.EntropyCoding"),
    lit("H265.BitrateControlType"),
    lit("H265.GOVLength"),
    lit("H265.PriorityType"),
    lit("H265.Profile"),
    lit("H265.EntropyCoding"),
};

bool HanwhaStreamLimits::setLimit(const HanwhaCgiParameter& parameter)
{
    if (!parameter.isValid())
        return false;

    auto limitName = parameter.name();

    if (limitName == lit("Resolution"))
        setCollectionLimits<QSize>(parameter, &resolutions);
    else if (limitName == lit("FrameRate"))
        setCollectionLimits(parameter, &frameRates);
    else if (limitName == lit("Bitrate"))
        setIntegerLimits(parameter, &bitrateLimits);
    else if (limitName == lit("EncodingType"))
        setCollectionLimits<AVCodecID>(parameter, &codecs);
    else if (limitName.endsWith(".BitrateControlType"))
        setCollectionLimits<Qn::BitrateControl>(parameter, &bitrateControlTypes);
    else if (limitName.endsWith(".GOVLength"))
        setIntegerLimits(parameter, &govLengthLimits);
    else if (limitName.endsWith(".PriorityType"))
        setCollectionLimits<Qn::EncodingPriority>(parameter, &encodingPriorityTypes);
    else if (limitName.endsWith(".EntropyCoding"))
        setCollectionLimits<Qn::EntropyCoding>(parameter, &entropyCodingTypes);
    else if (limitName == lit("H264.Profile"))
        h264Profiles = parameter.possibleValues();
    else if (limitName == lit("H265.Profile"))
        hevcProfiles = parameter.possibleValues();

    return true;
}

bool HanwhaStreamLimits::isValid() const
{
    return !resolutions.empty()
        && !frameRates.empty()
        && !codecs.empty()
        && bitrateLimits != kInvalidIntegerLimits
        && govLengthLimits != kInvalidIntegerLimits;

    // Ignore lack of the other parameters
}

template <typename Element, typename Collection>
bool HanwhaStreamLimits::setCollectionLimits(
    const HanwhaCgiParameter& parameter,
    Collection* collection)
{
    if (parameter.type() == HanwhaCgiParameterType::enumeration)
    {
        collection->clear();
        for (const auto& value: parameter.possibleValues())
            collection->insert(collection->end(), fromHanwhaString<Element>(value));

        return true;
    }
    
    return false;
}

bool HanwhaStreamLimits::setCollectionLimits(
    const HanwhaCgiParameter& parameter,
    std::vector<int>* collection)
{
    if (parameter.type() == HanwhaCgiParameterType::integer)
    {
        collection->clear();
        for (auto i = parameter.min(); i < parameter.max() + 1; ++i)
            collection->insert(collection->end(), i);

        return true;
    }

    return false;
}

bool HanwhaStreamLimits::setIntegerLimits(
    const HanwhaCgiParameter& parameter,
    std::pair<int, int>* integerLimits)
{
    if (parameter.type() != HanwhaCgiParameterType::integer)
        return false;

    *integerLimits = parameter.range();

    return true;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
