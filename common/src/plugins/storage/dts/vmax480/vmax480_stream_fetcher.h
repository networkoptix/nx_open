#ifndef __VMAX480_STREAM_FETCHER_H__
#define __VMAX480_STREAM_FETCHER_H__

#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"

class VMaxStreamFetcher
{
public:
    VMaxStreamFetcher(QnResourcePtr dev );

    virtual void onGotData(QnAbstractMediaDataPtr mediaData) = 0;

    void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime);

    bool isOpened() const;
protected:
    bool vmaxConnect(bool isLive);
    void vmaxDisconnect();
    void vmaxArchivePlay(qint64 timeUsec, quint8 sequence);
private:
    int getPort();
private:
    QProcess* m_vMaxProxy;
    QString m_tcpID;
    QnNetworkResourcePtr m_res;
};

#endif // __VMAX480_STREAM_FETCHER_H__
