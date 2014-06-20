////////////////////////////////////////////////////////////
// 7 feb 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_PARAMS_H
#define STREAMING_PARAMS_H

#include <QLatin1String>


//!Names of parameters, that can be specified by a client to get region of asset or apply some transcoding
namespace StreamingParams
{
    static const QLatin1String CHANNEL_PARAM_NAME( "channel" );
    //!Internal timestamp (in micros). Not a calendar time
    static const QLatin1String START_TIMESTAMP_PARAM_NAME( "startTimestamp" );
    //!Start calendar time
    static const QLatin1String START_DATETIME_PARAM_NAME( "startDatetime" );
    //static const QLatin1String STOP_TIMESTAMP_PARAM_NAME( "endTimestamp" );
    static const QLatin1String PICTURE_SIZE_PIXELS_PARAM_NAME( "pictureSizePixels" );
    static const QLatin1String CONTAINER_FORMAT_PARAM_NAME( "containerFormat" );
    static const QLatin1String VIDEO_CODEC_PARAM_NAME( "videoCodec" );
    static const QLatin1String AUDIO_CODEC_PARAM_NAME( "audioCodec" );
    static const QLatin1String DURATION_MS_PARAM_NAME( "duration" );
    static const QLatin1String ALIAS_PARAM_NAME( "alias" );
    static const QLatin1String LIVE_PARAM_NAME( "live" );
    //!Used to differ playlist with reference to other playlists from playlist with chunks
    static const QLatin1String CHUNKED_PARAM_NAME( "chunked" );
    static const QLatin1String SESSION_ID_PARAM_NAME( "sessionID" );
    static const QLatin1String HI_QUALITY_PARAM_NAME( "hi" );
    static const QLatin1String LO_QUALITY_PARAM_NAME( "lo" );
}

#endif  //STREAMING_PARAMS_H
