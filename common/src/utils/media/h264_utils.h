#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"

namespace nx {
namespace media_utils {
namespace avc {

/*!
    Adds \a Qn::PROFILE_LEVEL_ID_PARAM_NAME and \a Qn::SPROP_PARAMETER_SETS_PARAM_NAME parameters to \a customStreamParams.
    Those are suitable for SDP
*/
void extractSpsPps(
    const QnConstCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams);

std::vector<std::pair<const quint8*, size_t>> decodeNalUnits(const QnConstCompressedVideoDataPtr& videoData);

} // namespace avc
} // namespace media_utils
} // namespace nx
