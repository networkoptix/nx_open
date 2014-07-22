#ifndef AXIS_STREAM_REDER_H__
#define AXIS_STREAM_REDER_H__

#ifdef ENABLE_AXIS

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnAxisStreamReader: public CLServerPushStreamReader
{
public:
    QnAxisStreamReader(const QnResourcePtr& res);
    virtual ~QnAxisStreamReader();

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

    QnAbstractMediaDataPtr getNextDataMPEG(CodecID ci);
    QnAbstractMediaDataPtr getNextDataMJPEG();
    virtual QnMetaDataV1Ptr getCameraMetadata() override;

    QStringList getRTPurls() const;
    int toAxisQuality(Qn::StreamQuality quality);
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

#endif // #ifdef ENABLE_AXIS
#endif // AXIS_STREAM_REDER_H__
