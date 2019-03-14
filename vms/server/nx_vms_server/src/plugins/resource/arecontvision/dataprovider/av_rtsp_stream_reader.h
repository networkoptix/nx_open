/**********************************************************
* Oct 22, 2015
* a.kolesnikov
***********************************************************/

#pragma once

#ifdef ENABLE_ARECONT

#include <providers/spush_media_stream_provider.h>
#include "network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

#include "basic_av_stream_reader.h"
#include "arecont_meta_reader.h"
#include "../resource/av_resource.h"

class QnArecontRtspStreamReader
:
    public QnBasicAvStreamReader<CLServerPushStreamReader>
{
    typedef QnBasicAvStreamReader<CLServerPushStreamReader> parent_type;

public:
    QnArecontRtspStreamReader(const QnPlAreconVisionResourcePtr& res);
    virtual ~QnArecontRtspStreamReader();

    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const override;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStreamInternal(
        bool isCameraControlRequired,
        const QnLiveStreamParams& params) override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;
    virtual void pleaseStop() override;
    virtual void pleaseReopenStream() override;
    virtual void beforeRun() override;

private:
    virtual bool needMetadata() override;
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

private:
    QnMulticodecRtpReader m_rtpStreamParser;
    std::unique_ptr<ArecontMetaReader> m_metaReader;
};

#endif // ENABLE_ARECONT
