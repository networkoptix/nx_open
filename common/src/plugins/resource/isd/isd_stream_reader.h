#ifndef isd_STREAM_REDER_H__1914
#define isd_STREAM_REDER_H__1914

#ifdef ENABLE_ISD

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnISDStreamReader: public CLServerPushStreamReader
{
public:
    QnISDStreamReader(const QnResourcePtr& res);
    virtual ~QnISDStreamReader();

    QnConstResourceAudioLayoutPtr getDPAudioLayout() const;

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual CameraDiagnostics::Result openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;

    virtual void pleaseStop() override;
private:

    
    
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    
private:
    QnMulticodecRtpReader m_rtpStreamParser;
};

#endif // #ifdef ENABLE_ISD
#endif // isd_STREAM_REDER_H__1914
