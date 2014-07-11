#ifndef ACTI_STREAM_REDER_H__
#define ACTI_STREAM_REDER_H__

#ifdef ENABLE_ACTI

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/multicodec_rtp_reader.h"
#include "core/resource/resource_media_layout.h"

class QnActiStreamReader: public CLServerPushStreamReader
{
public:
    QnActiStreamReader(const QnResourcePtr& res);
    virtual ~QnActiStreamReader();

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
    int getActiChannelNum() const;
    QString formatBitrateStr(int bitrateKbps) const;
    QString formatResolutionStr(const QSize& resolution) const;
private:
    QnMulticodecRtpReader m_multiCodec;
    QnActiResourcePtr m_actiRes;
};

#endif // #ifdef ENABLE_ACTI
#endif // ACTI_STREAM_REDER_H__
