#ifndef __VMAX480_STREAM_FETCHER_H__
#define __VMAX480_STREAM_FETCHER_H__

#include <QSet>

#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"
#include "vmax480_tcp_server.h"
#include "core/resource/network_resource.h"

class VMaxStreamFetcherPtr;

class QnVmax480DataConsumer
{
public:
    virtual ~QnVmax480DataConsumer() {}
    virtual int getChannel() const { return -1; }

    virtual void onGotData(QnAbstractMediaDataPtr mediaData) {}
    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) {}
    virtual void onGotMonthInfo(const QDate& month, int monthInfo)  {}
    virtual void onGotDayInfo(int dayNum, const QByteArray& data)  {}
};

class QnVMax480ConnectionProcessor;

class VMaxStreamFetcher: public QnVmax480DataConsumer, public QnLongRunnable
{
public:
    void registerConsumer(QnVmax480DataConsumer* consumer);
    void unregisterConsumer(QnVmax480DataConsumer* consumer);

    static VMaxStreamFetcher* getInstance(const QString& clientGroupID, QnResourcePtr res, bool isLive);
    static void freeInstance(const QString& clientGroupID, QnResourcePtr res, bool isLive);
public:
    VMaxStreamFetcher(QnResourcePtr dev, bool isLive);
    virtual ~VMaxStreamFetcher();

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) override;
    virtual void onGotMonthInfo(const QDate& month, int monthInfo) override;
    virtual void onGotDayInfo(int dayNum, const QByteArray& data) override;

    void onConnectionEstablished(QnVMax480ConnectionProcessor* conection);
    virtual void onGotData(QnAbstractMediaDataPtr mediaData) override;

    void inUse();
    void notInUse();
    int usageCount() const { return m_usageCount; }
public:
    bool vmaxArchivePlay(QnVmax480DataConsumer* consumer, qint64 timeUsec, quint8 sequence, int speed);
    bool vmaxPlayRange(QnVmax480DataConsumer* consumer, const QList<qint64>& pointsUsec, quint8 sequence);

    bool vmaxRequestMonthInfo(const QDate& month);
    bool vmaxRequestDayInfo(int dayNum); // dayNum at vMax internal format
    bool vmaxRequestRange();
protected:
    virtual void run() override;
private:
    bool isOpened() const;
    bool vmaxConnect();
    void vmaxDisconnect();
    int getPort();
    int getCurrentChannelMask() const;
    bool waitForConnected();
    void doExtraDelay();
protected:
    QnNetworkResourcePtr m_res;
private:
    QMutex m_mutex;
    QProcess* m_vMaxProxy;
    QString m_tcpID;

    QMutex m_connectMtx;
    QWaitCondition m_vmaxConnectionCond;
    QnVMax480ConnectionProcessor* m_vmaxConnection;
    QSet<QnVmax480DataConsumer*> m_dataConsumers;
    bool m_isLive;
    int m_usageCount;

    static QMutex m_instMutex;
    static QMap<QString, VMaxStreamFetcher*> m_instances;
};

#endif // __VMAX480_STREAM_FETCHER_H__
