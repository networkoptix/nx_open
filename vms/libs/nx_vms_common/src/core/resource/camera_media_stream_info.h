// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavformat/avformat.h>
}

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/types/motion_types.h>

class NX_VMS_COMMON_API CameraMediaStreamInfo
{
public:
    static const QLatin1String anyResolution;
    static QString resolutionToString( const QSize& resolution = QSize() );

    nx::vms::api::StreamIndex getEncoderIndex() const;

    // We have to keep compatibility with previous version. So, this field stay int
    int encoderIndex;
    //!has format "1920x1080" or "*" to notify that any resolution is supported
    QString resolution;
    //!transport method that can be used to receive this stream
    /*!
    Possible values:\n
    - rtsp
    - webm (webm over http)
    - hls (Apple Http Live Streaming)
    - mjpeg (motion jpeg over http)
    - webrtc
    */
    std::vector<QString> transports;
    //!if \a true this stream is produced by transcoding one of native (having this flag set to \a false) stream
    bool transcodingRequired;
    int codec;
    std::map<QString, QString> customStreamParams; // TODO remove outdated field

    CameraMediaStreamInfo(
        nx::vms::api::StreamIndex encoderIndex = nx::vms::api::StreamIndex::undefined,
        const QSize& _resolution = QSize(),
        AVCodecID _codec = AV_CODEC_ID_NONE)
        :
        encoderIndex((int) encoderIndex ),
        resolution( resolutionToString( _resolution ) ),
        transcodingRequired( false ),
        codec( _codec )
    {
        //TODO #akolesnikov delegate to next constructor after moving to vs2013
    }

    bool operator==( const CameraMediaStreamInfo& rhs ) const;
    bool operator!=( const CameraMediaStreamInfo& rhs ) const;
    QSize getResolution() const;
};

#define CameraMediaStreamInfo_Fields (encoderIndex)(resolution)(transports)(transcodingRequired)(codec)(customStreamParams)


class NX_VMS_COMMON_API CameraMediaStreams
{
public:
    std::vector<CameraMediaStreamInfo> streams;
};
#define CameraMediaStreams_Fields (streams)

class NX_VMS_COMMON_API CameraBitrateInfo
{
public:
    nx::vms::api::StreamIndex encoderIndex;
    QString timestamp;

    float rawSuggestedBitrate = -1; //< Megabits per second
    float suggestedBitrate = -1; //< Megabits per second
    float actualBitrate = -1; //< Megabits per second. Current bitrate value.

    bool bitratePerGop = false;
    float bitrateFactor = -1;

    int fps = -1;
    float actualFps = -1;
    float averageGopSize;
    QString resolution;
    int numberOfChannels = -1;
    bool isConfigured = false;
    float avarageBitrateMbps = -1; //< Average bitrate for a whole archive.

    CameraBitrateInfo(
        nx::vms::api::StreamIndex index = nx::vms::api::StreamIndex::undefined,
        QString time = QString())
        :
        encoderIndex(index),
        timestamp(time)
    {
    }
};
#define CameraBitrateInfo_Fields (encoderIndex)(timestamp) \
    (rawSuggestedBitrate)(suggestedBitrate)(actualBitrate) \
    (bitratePerGop)(bitrateFactor) \
    (fps)(actualFps)(averageGopSize)(resolution)(numberOfChannels)(isConfigured)(avarageBitrateMbps)

class NX_VMS_COMMON_API CameraBitrates
{
public:
    std::vector<CameraBitrateInfo> streams;
};
#define CameraBitrates_Fields (streams)

QN_FUSION_DECLARE_FUNCTIONS(CameraMediaStreamInfo, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CameraMediaStreams, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CameraBitrateInfo, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(CameraBitrates, (json), NX_VMS_COMMON_API)
