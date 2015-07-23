#ifndef __VMAX480_CHUNK_READER_H__
#define __VMAX480_CHUNK_READER_H__

#ifdef ENABLE_VMAX

#include "plugins/resource/archive/abstract_archive_delegate.h"
#include "vmax480_stream_fetcher.h"
#include "utils/common/long_runnable.h"
#include "recording/time_period_list.h"
#include "../../../../vmaxproxy/src/vmax480_helper.h"

class QnVMax480ChunkReader: public  QnLongRunnable, public QnVmax480DataConsumer
{
    Q_OBJECT;
public:
    QnVMax480ChunkReader(const QnResourcePtr& res);
    virtual ~QnVMax480ChunkReader();

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) override;
    virtual void onGotMonthInfo(const QDate& month, int monthInfo) override;
    virtual void onGotDayInfo(int dayNum, const QByteArray& data) override;

signals:
    void gotChunks(int channel, QnTimePeriodList chunks);
protected:
    virtual void run() override;
private:
    void updateRecordedDays(quint32 startDateTime, quint32 endDateTime);
    void addChunk(QnTimePeriodList& chunks, const QnTimePeriod& period);
private:
    enum State {State_Started, State_ReadDays, State_ReadTime, State_ReadRange, State_UpdateData};

    QnMutex m_mutex;
    QQueue<int> m_reqMonths;
    QnTimePeriodList m_chunks[VMAX_MAX_CH];
    QQueue<QDate> m_monthToRequest;
    QQueue<int> m_daysToRequest;
    bool m_waitingAnswer;
    State m_state;
    QElapsedTimer m_updateTimer;
    bool m_firstRange;
    QnTimePeriod m_archiveRange;
    QElapsedTimer m_waitTimer;
    VMaxStreamFetcher* m_streamFetcher;
    QnResource* m_res;
    bool m_gotAllData;
};

#endif // #ifdef ENABLE_VMAX
#endif // __VMAX480_CHUNK_READER_H__
