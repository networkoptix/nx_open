#ifndef NX_H264_UTILS_H
#define NX_H264_UTILS_H


#include <map>

#include <QtCore/QSize>
#include <QtCore/QString>

#include "nx/streaming/video_data_packet.h"


/*!
    Adds \a Qn::PROFILE_LEVEL_ID_PARAM_NAME and \a Qn::SPROP_PARAMETER_SETS_PARAM_NAME parameters to \a customStreamParams.
    Those are suitable for SDP
*/
void extractSpsPps(
    const QnConstCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams);

#endif  //NX_H264_UTILS_H
