#ifndef __VMAX480_CHUNK_READER_H__
#define __VMAX480_CHUNK_READER_H__

#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "vmax480_stream_fetcher.h"
#include "utils/common/long_runnable.h"
#include "recording/time_period_list.h"
#include "../../../../vmaxproxy/src/vmax480_helper.h"

class QnVMax480ChunkReader: public  QnLongRunnable, public VMaxStreamFetcher
{
    Q_OBJECT;
public:
    QnVMax480ChunkReader(QnResourcePtr res);
    virtual ~QnVMax480ChunkReader();

    virtual void onGotArchiveRange(quint32 startDateTime, quint32 endDateTime) override;
    virtual void onGotMonthInfo(const QDate& month, int monthInfo) override;
    virtual void onGotDayInfo(int dayNum, const QByteArray& data) override;
signals:
    void gotChunks(int channel, QnTimePeriodList chunks);
protected:
    virtual void run() override;
private:
    void updateRecordedDays();
    void addChunk(QnTimePeriodList& chunks, const QnTimePeriod& period);
private:
    enum State {State_Started, State_ReadDays, State_ReadTime, State_Ready};

    QMutex m_mutex;
    quint32 m_startDateTime;
    quint32 m_endDateTime;
    QQueue<int> m_reqMonths;
    QnTimePeriodList m_chunks[VMAX_MAX_CH];
    QQueue<QDate> m_monthToRequest;
    QQueue<int> m_daysToRequest;
    bool m_waitingAnswer;
    State m_state;
};

#endif // __VMAX480_CHUNK_READER_H__
