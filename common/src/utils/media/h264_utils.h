#pragma once

#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"

namespace nx {
namespace media_utils {

void readNALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits);

namespace h264 {

/*!
    Adds \a Qn::PROFILE_LEVEL_ID_PARAM_NAME and \a Qn::SPROP_PARAMETER_SETS_PARAM_NAME parameters to \a customStreamParams.
    Those are suitable for SDP
*/
void extractSpsPps(
    const QnConstCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams);

} // namespace h264

} // namespace media_utils
} // namespace nx
