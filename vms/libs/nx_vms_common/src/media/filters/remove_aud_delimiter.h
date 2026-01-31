// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////
#pragma once

#include <nx/media/media_data_packet.h>
#include <nx/media/video_data_packet.h>

#include "abstract_media_data_filter.h"

//!Remove AUD delimiter from h.264/h.265 streams
class NX_VMS_COMMON_API H2645RemoveAudDelimiter: public AbstractMediaDataFilter
{
public:
    //!Implementation of AbstractMediaDataFilter::processData
    virtual QnConstAbstractDataPacketPtr processData(
        const QnConstAbstractDataPacketPtr& data) override;

    // Returns nullptr if the video frame cannot be converted, otherwise the converted frame.
    QnCompressedVideoDataPtr processVideoData(const QnConstCompressedVideoDataPtr& data);

private:

    QnCompressedVideoDataPtr removeAuxFromAnexBFormat(
        const QnConstCompressedVideoDataPtr& videoData);

    QnCompressedVideoDataPtr removeAuxFromMp4Format(
        const QnConstCompressedVideoDataPtr& videoData);

    CodecParametersConstPtr m_newContext;
};
