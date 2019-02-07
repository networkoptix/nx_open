#pragma once

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <QtCore/QSize>

#include <nx/vms/api/types/motion_types.h>
#include <nx/fusion/model_functions_fwd.h>

class CameraMediaStreamInfo
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
    - mjpg (motion jpeg over http)
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
        //TODO #ak delegate to next constructor after moving to vs2013
    }

    bool operator==( const CameraMediaStreamInfo& rhs ) const;
    bool operator!=( const CameraMediaStreamInfo& rhs ) const;
    QSize getResolution() const;
};

#define CameraMediaStreamInfo_Fields (encoderIndex)(resolution)(transports)(transcodingRequired)(codec)(customStreamParams)


class CameraMediaStreams
{
public:
    std::vector<CameraMediaStreamInfo> streams;
};
#define CameraMediaStreams_Fields (streams)

class CameraBitrateInfo
{
public:
    nx::vms::api::StreamIndex encoderIndex;
    QString timestamp;

    float rawSuggestedBitrate = -1; //< Megabits per second
    float suggestedBitrate = -1; //< Megabits per second
    float actualBitrate = -1; //< Megabits per second

    bool bitratePerGop = false;
    float bitrateFactor = -1;

    int fps = -1;
    float actualFps = -1;
    float averageGopSize;
    QString resolution;
    int numberOfChannels = -1;
    bool isConfigured = false;

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
    (fps)(actualFps)(averageGopSize)(resolution)(numberOfChannels)(isConfigured)

class CameraBitrates
{
public:
    std::vector<CameraBitrateInfo> streams;
};
#define CameraBitrates_Fields (streams)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(\
(CameraMediaStreamInfo)(CameraMediaStreams) \
(CameraBitrateInfo)(CameraBitrates), \
(json))

