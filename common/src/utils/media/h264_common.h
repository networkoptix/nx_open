#pragma once

namespace nx {
namespace media_utils {
namespace h264 {

enum class Profile
{
    undefined = 0,
    constrainedBaseline,
    baseline,
    extended,
    main,
    high,
    progressiveHigh,
    constrainedHigh,
    high10,
    high422,
    high444,
    high10Intra,
    high422Intra,
    high444Intra,
    cavlcIntra,
    scalableBaseline,
    scalableConstrainedBaseline,
    scalableHigh,
    scalableConstrainedHigh,
    scalableHighIntra,
    stereoHigh,
    multiviewHigh,
    multiviewDepthHigh
};

} // namespace h264
} // namespace media_utils
} // namespace nx