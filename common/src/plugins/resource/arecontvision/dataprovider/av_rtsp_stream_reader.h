/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_ARECONT_RTSP_STREAM_READER_H
#define NX_ARECONT_RTSP_STREAM_READER_H

#ifdef ENABLE_ARECONT

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

#include "../resource/av_resource.h"


class QnArecontRtspStreamReader
:
    public CLServerPushStreamReader
{
public:
    QnArecontRtspStreamReader(const QnResourcePtr& res);
    virtual ~QnArecontRtspStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;

private:
    QnMulticodecRtpReader m_rtpStreamParser;

    virtual QnMetaDataV1Ptr getCameraMetadata() override;
    QString generateRequestString(
        const QnLiveStreamParams& params,
        const QnPlAreconVisionResourcePtr& res);
};

#endif // ENABLE_ARECONT

#endif // NX_ARECONT_RTSP_STREAM_READER_H
