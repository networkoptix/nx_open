#ifndef __RTSP_CLIENT_DATA_PROVIDER_H
#define __RTSP_CLIENT_DATA_PROVIDER_H

#include "dataprovider/navigated_dataprovider.h"
#include "network/rtpsession.h"
#include "resource/client/client_media_resource.h"

class QnRtspClientDataProvider: public QnNavigatedDataProvider
{
public:
    QnRtspClientDataProvider(QnResourcePtr res);
    virtual ~QnRtspClientDataProvider();
protected:
    virtual QnAbstractDataPacketPtr getNextData();
    virtual void updateStreamParamsBasedOnQuality(); 
    virtual void channeljumpTo(quint64 mksec, int channel);
private:
    QnAbstractDataPacketPtr processFFmpegRtpPayload(const quint8* data, int dataSize);
private:
    RTPSession m_rtspSession;
    RTPIODevice* m_rtpData;
    bool m_isOpened;
    quint8* m_rtpDataBuffer;
    bool m_tcpMode;
    QMap<int, AVCodecContext*> m_contextMap;
    QMap<int, QnAbstractMediaDataPacketPtr> m_nextDataPacket;
};

#endif
