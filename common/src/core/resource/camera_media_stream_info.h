#pragma once

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>

class CameraMediaStreamInfo
{
public:
    static const int PRIMARY_STREAM_INDEX = 0;
    static const int SECONDARY_STREAM_INDEX = 1;

    static const QLatin1String anyResolution;
    static QString resolutionToString( const QSize& resolution = QSize() );

    //!0 - primary stream, 1 - secondary stream
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
    std::map<QString, QString> customStreamParams;

    CameraMediaStreamInfo(
        int _encoderIndex = -1,
        const QSize& _resolution = QSize(),
        AVCodecID _codec = AV_CODEC_ID_NONE)
        :
        encoderIndex( _encoderIndex ),
        resolution( resolutionToString( _resolution ) ),
        transcodingRequired( false ),
        codec( _codec )
    {
        //TODO #ak delegate to next constructor after moving to vs2013
    }

    template<class CustomParamDictType>
    CameraMediaStreamInfo(
        int _encoderIndex,
        const QSize& _resolution,
        AVCodecID _codec,
        CustomParamDictType&& _customStreamParams )
        :
        encoderIndex( _encoderIndex ),
        resolution( _resolution.isValid()
            ? QString::fromLatin1("%1x%2").arg(_resolution.width()).arg(_resolution.height())
            : anyResolution ),
        transcodingRequired( false ),
        codec( _codec ),
        customStreamParams( std::forward<CustomParamDictType>(_customStreamParams) )
    {
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
    int encoderIndex;
    QString timestamp;

    float rawSuggestedBitrate;  //< Megabits per second
    float suggestedBitrate;     //< Megabits per second
    float actualBitrate;        //< Megabits per second

    Qn::BitratePerGopType bitratePerGop;
    float bitrateFactor;

    int fps;
    float actualFps;
    float averageGopSize;
    QString resolution;
    int numberOfChannels;
    bool isConfigured;

    CameraBitrateInfo(int index = -1, QString time = QString())
        : encoderIndex(index)
        , timestamp(time)
        , rawSuggestedBitrate(-1)
        , suggestedBitrate(-1)
        , actualBitrate(-1)
        , bitratePerGop(Qn::BPG_None)
        , bitrateFactor(-1)
        , fps(-1)
        , actualFps(-1)
        , numberOfChannels(-1)
        , isConfigured(false)
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

