#include <QDateTime>

#include "vmax480_chunk_reader.h"
#include "utils/common/synctime.h"

static const QDate MAX_ARCHIVE_DATE(2200, 1, 1);

QnVMax480ChunkReader::QnVMax480ChunkReader(QnResourcePtr res):
    VMaxStreamFetcher(res),
    QnLongRunnable(),
    m_waitingAnswer(false),
    m_state(State_Started),
    m_firstRange(true)
{

}

QnVMax480ChunkReader::~QnVMax480ChunkReader()
{

}

void QnVMax480ChunkReader::run()
{
    while (!m_needStop)
    {
        if (!isOpened()) {
            vmaxConnect(false, -1);
            if (isOpened()) {
                m_waitingAnswer = true;
                m_waitTimer.restart();
                vmaxRequestRange();
            }
            else {
                msleep(10000);
                continue;
            }
        }

        if (m_waitingAnswer) 
        {
            if (m_waitTimer.elapsed() > 1000*3000) {
                vmaxDisconnect();
                m_state = State_Started;
                m_waitTimer.restart();
                m_waitingAnswer = false;
                m_firstRange = true;
            }
            msleep(1);
            continue;
        }

        QMutexLocker lock(&m_mutex);

        switch (m_state)
        {
        case State_ReadDays:
            if (!m_monthToRequest.isEmpty()) {
                m_waitingAnswer = true;
                m_waitTimer.restart();
                m_updateTimer.restart();
                vmaxRequestMonthInfo(m_monthToRequest.dequeue());
                break;
            }
            m_state = State_ReadTime;

        case State_ReadTime:
            m_waitingAnswer = true;
            m_waitTimer.restart();
            if (!m_daysToRequest.isEmpty()) {
                vmaxRequestDayInfo(m_daysToRequest.dequeue());
            }
            else {
                vmaxRequestRange();
                m_state = State_ReadRange;
            }
            break;

        case State_ReadRange:
            for (int i = 0; i < VMAX_MAX_CH; ++i) {
                m_chunks[i] = m_chunks[i].intersected(m_archiveRange);
                emit gotChunks(i, m_chunks[i]);
            }
            m_updateTimer.restart();
            m_state = State_UpdateData;
            break;
            
        case State_UpdateData: 
            if (m_updateTimer.elapsed() > 60*1000)
            {
                QDate date = qnSyncTime->currentDateTime().date();
                int dayNum = date.year()*10000 + date.month()*100 + date.day();
                m_waitingAnswer = true;
                m_waitTimer.restart();
                vmaxRequestDayInfo(dayNum);
                m_state = State_ReadTime;
            }
            break;
        }

        msleep(1);
    }
}

void QnVMax480ChunkReader::updateRecordedDays(quint32 startDateTime, quint32 endDateTime)
{
    QDate startDate;
    QDate endDate;

    startDate = QDateTime::fromMSecsSinceEpoch(startDateTime*1000ll).date();
    endDate = QDateTime::fromMSecsSinceEpoch(endDateTime*1000ll).date();

    startDate = startDate.addDays(-(startDate.day()-1));
    endDate = endDate.addDays(-(endDate.day()-1));

    while (startDate <= endDate) {
        m_monthToRequest << startDate;
        startDate = startDate.addMonths(1);
    }

    if (!m_monthToRequest.isEmpty())
        m_state = State_ReadDays;
    else
        m_state = State_UpdateData;
}

void QnVMax480ChunkReader::onGotArchiveRange(quint32 startDateTime, quint32 endDateTime)
{
    VMaxStreamFetcher::onGotArchiveRange(startDateTime, endDateTime);
    QMutexLocker lock(&m_mutex);
    if (m_firstRange) {
        updateRecordedDays(startDateTime, endDateTime);
        m_firstRange = false;
    }

    endDateTime = QDateTime(MAX_ARCHIVE_DATE).toMSecsSinceEpoch();
    m_archiveRange = QnTimePeriod(startDateTime * 1000ll, (endDateTime-startDateTime) * 1000ll);
    m_waitingAnswer = false;
}

void QnVMax480ChunkReader::onGotMonthInfo(const QDate& month, int _monthInfo)
{
    QMutexLocker lock(&m_mutex);

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
    QMutexLocker lock(&m_mutex);

    int year =  dayNum / 10000;
    int month =(dayNum % 10000)/100;
    int day =   dayNum % 100;
    qint64 dayBase = QDateTime(QDate(year, month, day)).toMSecsSinceEpoch();

    const char* curPtr = data.data();
    QnTimePeriodList dayPeriods;

    for (int ch = 0; ch < VMAX_MAX_CH; ++ch)
    {
        for(int min = 0; min < VMAX_MAX_SLICE_DAY; ++min)
        {
            char recordType = *curPtr & 0x0f;
            if (recordType)
                addChunk(dayPeriods, QnTimePeriod(dayBase + min*60*1000ll, 60*1000));
            curPtr++;
        }

        if (!dayPeriods.isEmpty()) {
            QVector<QnTimePeriodList> allPeriods;
            allPeriods << m_chunks[ch];
            allPeriods << dayPeriods;
            m_chunks[ch] = QnTimePeriod::mergeTimePeriods(allPeriods);
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
