#ifndef isd_STREAM_REDER_H__1914
#define isd_STREAM_REDER_H__1914

#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnISDStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnISDStreamReader(QnResourcePtr res);
    virtual ~QnISDStreamReader();

    const QnResourceAudioLayout* getDPAudioLayout() const;
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
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

#endif // isd_STREAM_REDER_H__1914
