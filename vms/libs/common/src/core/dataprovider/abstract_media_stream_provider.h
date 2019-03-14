#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include "nx/streaming/media_data_packet.h"
#include "core/resource/resource_media_layout.h"
#include "utils/camera/camera_diagnostics.h"


class QnAbstractMediaStreamProvider
{
public:
    virtual ~QnAbstractMediaStreamProvider() {}

    enum class ErrorCode
    {
        noError,
        frameLost
    };

    virtual CameraDiagnostics::Result openStream() = 0;
    virtual CameraDiagnostics::Result lastOpenStreamResult() const = 0;
    virtual void closeStream() = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    virtual bool isStreamOpened() const = 0;
    //!
    /*!
        TODO #ak does not look appropriate here
    */
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() const { return QnConstResourceAudioLayoutPtr(); };
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const { return QnConstResourceVideoLayoutPtr(); }
};

#endif // ENABLE_DATA_PROVIDERS
