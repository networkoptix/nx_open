#ifndef AXIS_STREAM_REDER_H__
#define AXIS_STREAM_REDER_H__

#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/h264_rtp_reader.h"

class QnAxisStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnAxisStreamReader(QnResourcePtr res);
    virtual ~QnAxisStreamReader();

protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void openStream() override;
    virtual void closeStream() override;
    virtual bool isStreamOpened() const override;


    virtual void updateStreamParamsBasedOnQuality() override;
    virtual void updateStreamParamsBasedOnFps() override;


private:

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    QnMetaDataV1Ptr getMetaData() override;

    QStringList getRTPurls() const;
    int toAxisQuality(QnStreamQuality quality);

private:

    RTPH264StreamreaderDelegate m_RTP264;
};

#endif // AXIS_STREAM_REDER_H__
