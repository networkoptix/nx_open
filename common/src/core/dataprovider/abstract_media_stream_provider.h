/**********************************************************
* 2 june 2014
* akolesnikov
***********************************************************/

#ifndef ABSTRACT_MEDIA_STREAM_PROVIDER_H
#define ABSTRACT_MEDIA_STREAM_PROVIDER_H

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/media_data_packet.h"
#include "core/resource/resource_media_layout.h"
#include "utils/camera/camera_diagnostics.h"


class QnAbstractMediaStreamProvider
{
public:
    virtual ~QnAbstractMediaStreamProvider() {}

    /*!
        \return true, if successfully opened stream or stream is already opened. false, if failed. For more detail, call \a getLastResponseCode()
    */
    virtual CameraDiagnostics::Result openStream() = 0;
    virtual void closeStream() = 0;
    virtual QnAbstractMediaDataPtr getNextData() = 0;
    virtual bool isStreamOpened() const = 0;
    //!Returns last HTTP response code (even if used media protocol is not http)
    virtual int getLastResponseCode() const { return 0; };
    //!
    /*!
        TODO #ak does not look appropriate here
    */
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() const { return QnConstResourceAudioLayoutPtr(); };
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() const { return QnConstResourceVideoLayoutPtr(); }
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //ABSTRACT_MEDIA_STREAM_PROVIDER_H
