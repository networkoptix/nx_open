#ifndef __VMAX480_STREAM_FETCHER_H__
#define __VMAX480_STREAM_FETCHER_H__

#ifdef ENABLE_VMAX

#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QSet>
#include <utils/common/wait_condition.h>

#include "core/resource/resource_fwd.h"
#include "core/datapacket/media_data_packet.h"
#include "core/resource/network_resource.h"
#include "recording/time_period_list.h"
#include "recording/playbackmask_helper.h"

class VMaxStreamFetcherPtr;
class QnVmax480DataConsumer
{
public:
    virtual ~QnVmax480DataConsumer() {}
    virtual int getChannel() const { return -1; }

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) { Q_UNUSED(startDateTime) Q_UNUSED(endDateTime) }
    virtual void onGotMonthInfo(const QDate& month, int monthInfo)  { Q_UNUSED(month) Q_UNUSED(monthInfo) }
    virtual void onGotDayInfo(int dayNum, const QByteArray& data)  { Q_UNUSED(dayNum) Q_UNUSED(data) }

    virtual QnTimePeriodList chunks() { return QnTimePeriodList(); }
    virtual bool isStopping() const { return false; }
};


class QnVMax480ConnectionProcessor;

class VMaxStreamFetcher: public QnVmax480DataConsumer
{
public:
    bool registerConsumer(QnVmax480DataConsumer* consumer, int* count = 0, bool keepAllChannels = false, bool checkPlaybackMask = false);
    void unregisterConsumer(QnVmax480DataConsumer* consumer);

    static VMaxStreamFetcher* getInstance(const QByteArray& clientGroupID, QnResource* res, bool isLive);

    static void freeInstance(const QByteArray& clientGroupID, QnResource* res, bool isLive);

    QnAbstractDataPacketPtr getNextData(QnVmax480DataConsumer* consumer);
    static void pleaseStopAll();

public:
    VMaxStreamFetcher(QnResource* dev, bool isLive);
    virtual ~VMaxStreamFetcher();

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) override;
    virtual void onGotMonthInfo(const QDate& month, int monthInfo) override;
    virtual void onGotDayInfo(int dayNum, const QByteArray& data) override;

    void onConnectionEstablished(QnVMax480ConnectionProcessor* conection);
    void onGotData(QnAbstractMediaDataPtr mediaData);

    void inUse();
    void notInUse();
    int usageCount() const { return m_usageCount; }
    bool isEOF() const;
public:
    bool vmaxArchivePlay(QnVmax480DataConsumer* consumer, qint64 timeUsec, int speed);
    bool vmaxPlayRange(QnVmax480DataConsumer* consumer, const QList<qint64>& pointsUsec);

    bool vmaxRequestMonthInfo(const QDate& month);
    bool vmaxRequestDayInfo(int dayNum); // dayNum at vMax internal format
    bool vmaxRequestRange();
    QnAbstractMediaDataPtr createEmptyPacket(qint64 timestamp);
    bool isPlaying() const;
private:
    bool isOpened() const;
    bool vmaxConnect();
    void vmaxDisconnect();
    int getPort();
    int getCurrentChannelMask() const;
    int getChannelUsage(int ch);
    int getMaxQueueSize() const;
    bool safeOpen();
    qint64 findRoundTime(qint64 timeUsec, bool* dataFound) const;
    void updatePlaybackMask();
    void initPacketTime();
    void checkEOF(qint64 timestamp);
    void pleaseStop();
private:
    static const int OPEN_ALL = 0xffff;

    QnNetworkResource* m_res;
    typedef QMap<QnVmax480DataConsumer*, CLDataQueue*> ConsumersMap;

    mutable QnMutex m_mutex;
    QProcess* m_vMaxProxy;
    QString m_tcpID;

    QnMutex m_connectMtx;
    QnWaitCondition m_vmaxConnectionCond;
    QnVMax480ConnectionProcessor* m_vmaxConnection;
    ConsumersMap m_dataConsumers;
    bool m_isLive;
    int m_usageCount;

    static QnMutex m_instMutex;
    static QMap<QByteArray, VMaxStreamFetcher*> m_instances;
    quint8 m_sequence;
    qint64 m_lastChannelTime[256];
    qint64 m_lastMediaTime;
    qint64 m_emptyPacketTime;
    bool m_streamPaused;
    int m_lastSpeed;
    qint64 m_lastSeekPos;
    bool m_beforeSeek;
    QElapsedTimer m_seekTimer;
    bool m_isPlaying;
    bool m_keepAllChannels;
    QnPlaybackMaskHelper m_playbackMaskHelper;
    qint64 m_lastConnectTimeUsec;
    bool m_eofReached;
    bool m_needStop;
};

#endif //#ifdef ENABLE_VMAX
#endif // __VMAX480_STREAM_FETCHER_H__
