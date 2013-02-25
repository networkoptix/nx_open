#ifndef __VMAX480_STREAM_FETCHER_H__
#define __VMAX480_STREAM_FETCHER_H__

#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"

class QnVMax480ConnectionProcessor;

class VMaxStreamFetcher
{
public:
    VMaxStreamFetcher(QnResourcePtr dev );
    virtual ~VMaxStreamFetcher();

    virtual void onGotData(QnAbstractMediaDataPtr mediaData) {}

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime);
    virtual void onGotMonthInfo(const QDate& month, int monthInfo) {}
    virtual void onGotDayInfo(int dayNum, const QByteArray& data) {}

    bool isOpened() const;

    void onConnectionEstablished(QnVMax480ConnectionProcessor* conection);
protected:
    bool vmaxConnect(bool isLive, int channel);
    void vmaxDisconnect();
    void vmaxArchivePlay(qint64 timeUsec, quint8 sequence);
    void vmaxRequestMonthInfo(const QDate& month);
    void vmaxRequestDayInfo(int dayNum); // dayNum at vMax internal format
    void vmaxRequestRange();
private:
    int getPort();
protected:
    QnNetworkResourcePtr m_res;
private:
    QProcess* m_vMaxProxy;
    QString m_tcpID;

    QMutex m_connectMtx;
    QWaitCondition m_vmaxConnectionCond;
    QnVMax480ConnectionProcessor* m_vmaxConnection;
};

#endif // __VMAX480_STREAM_FETCHER_H__
