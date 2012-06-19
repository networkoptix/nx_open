#ifndef AXIS_STREAM_REDER_H__
#define AXIS_STREAM_REDER_H__

#include "core/dataprovider/live_stream_provider.h"
#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnAxisStreamReader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
    QnAxisStreamReader(QnResourcePtr res);
    virtual ~QnAxisStreamReader();

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

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    int toAxisQuality(QnStreamQuality quality);
    void parseMotionInfo(QnCompressedVideoDataPtr videoData);
    void processTriggerData(const quint8* payload, int len);

    void fillMotionInfo(const QRect& rect);
    bool isGotFrame(QnCompressedVideoDataPtr videoData);
private:
    QnMetaDataV1Ptr m_lastMetadata;
    QnMulticodecRtpReader m_rtpStreamParser;
    QnPlAxisResourcePtr m_axisRes;

    bool m_oldFirmwareWarned;
};

#endif // AXIS_STREAM_REDER_H__
