#include <QDateTime>

#include "vmax480_chunk_reader.h"
#include "vmax480_stream_fetcher.h"
#include "vmax480_resource.h"

QnVMax480ChunkReader::QnVMax480ChunkReader(QnResourcePtr res):
    VMaxStreamFetcher(res),
    QnLongRunnable(),
    m_startDateTime(0),
    m_endDateTime(0),
    m_waitingAnswer(false),
    m_state(State_Started)
{

}

QnVMax480ChunkReader::~QnVMax480ChunkReader()
{

}

void QnVMax480ChunkReader::run()
{
    while (!m_needStop)
    {
        if (!isOpened())
            vmaxConnect(false, -1);
        if (!isOpened()) {
            msleep(1000);
            continue;
        }
        updateRecordedDays();

        if (!m_waitingAnswer) 
        {
            if (m_state == State_ReadDays)
            {
                if (!m_monthToRequest.isEmpty()) {
                    m_waitingAnswer = true;
                    vmaxRequestMonthInfo(m_monthToRequest.dequeue());
                }
                else {
                    m_state = State_ReadTime;
                }
            }

            if (m_state == State_ReadTime)
            {
                if (!m_daysToRequest.isEmpty()) {
                    m_waitingAnswer = true;
                    vmaxRequestDayInfo(m_daysToRequest.dequeue());
                }
                else {
                    m_state = State_Ready;
                    for (int i = 0; i < VMAX_MAX_CH; ++i)
                        emit gotChunks(i, m_chunks[i]);
                }
            }
        }

        msleep(1);
    }
}

void QnVMax480ChunkReader::updateRecordedDays()
{
    QDate startDate;
    QDate endDate;
    {
        QMutexLocker lock(&m_mutex);
        if (m_startDateTime == 0)
            return;
        startDate = QDateTime::fromMSecsSinceEpoch(m_startDateTime*1000ll).date();
        endDate = QDateTime::fromMSecsSinceEpoch(m_endDateTime*1000ll).date();
        m_startDateTime = m_endDateTime = 0;

        startDate = startDate.addDays(-(startDate.day()-1));
        endDate = endDate.addDays(-(endDate.day()-1));
        while (startDate <= endDate) {
            m_monthToRequest << startDate;
            startDate = startDate.addMonths(1);
        }
        m_state = State_ReadDays;
    }
}

void QnVMax480ChunkReader::onGotArchiveRange(quint32 startDateTime, quint32 endDateTime)
{
    m_res.dynamicCast<QnPlVmax480Resource>()->setArchiveRange(startDateTime * 1000000ll, endDateTime * 1000000ll);

    QMutexLocker lock(&m_mutex);
    m_startDateTime = startDateTime;
    m_endDateTime = endDateTime;
}

void QnVMax480ChunkReader::onGotMonthInfo(const QDate& month, int _monthInfo)
{
    QDateTime timestamp(month);
    qint64 base = timestamp.toMSecsSinceEpoch();
    quint32 monthInfo = (quint32) _monthInfo;
    for (int i = 0; i < 31; ++i)
    {
        if (monthInfo & 1) {
            int dayNum = month.year()*10000 + month.month()*100 + i + 1;
            m_daysToRequest << dayNum;
        }
        monthInfo >>= 1;
    }

    m_waitingAnswer = false;
}

void QnVMax480ChunkReader::onGotDayInfo(int dayNum, const QByteArray& data)
{
    int year =  dayNum / 10000;
    int month =(dayNum % 10000)/100;
    int day =   dayNum % 100;
    qint64 dayBase = QDateTime(QDate(year, month, day)).toMSecsSinceEpoch();

    const char* curPtr = data.data();
    for (int ch = 0; ch < VMAX_MAX_CH; ++ch)
    {
        for(int min = 0; min < VMAX_MAX_SLICE_DAY; ++min)
        {
            char recordType = *curPtr & 0x0f;
            if (recordType) {
                addChunk(m_chunks[ch], QnTimePeriod(dayBase + min*60*1000ll, 60*1000));
            }
            curPtr++;
        }
    }
    m_waitingAnswer = false;
}

void QnVMax480ChunkReader::addChunk(QnTimePeriodList& chunks, const QnTimePeriod& period)
{
    if (!chunks.isEmpty() && chunks.last().endTimeMs() == period.startTimeMs)
        chunks.last().addPeriod(period);
    else
        chunks << period;
}
