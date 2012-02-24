#include "time_period_reader.h"

int QnTimePeriodReader::m_fakeHandle(INT_MAX/2);

QnTimePeriodReader::QnTimePeriodReader(const QnVideoServerConnectionPtr &connection, QnNetworkResourcePtr resource, QObject *parent):
    QObject(parent), 
    m_connection(connection), 
    m_resource(resource)
{
    m_fakeHandle = INT_MAX/2;
    qRegisterMetaType<QnTimePeriodList>("QnTimePeriodList");
    connect(this, SIGNAL(delayedReady(const QnTimePeriodList&, int)), this, SIGNAL(ready(const QnTimePeriodList&, int)), Qt::QueuedConnection);
}

int QnTimePeriodReader::load(const QnTimePeriod &timePeriod, const QList<QRegion>& regions)
{
    QMutexLocker lock(&m_mutex);
    if (regions != m_regions) {
        m_loadedPeriods.clear();
        m_loadedData.clear();
    }
    m_regions = regions;

    foreach(const QnTimePeriod& loadedPeriod, m_loadedPeriods) 
    {
        if (loadedPeriod.containPeriod(timePeriod)) 
        {
            // data already loaded
            m_fakeHandle++;
            //emit ready(m_loadedData, m_fakeHandle);
            emit delayedReady(m_loadedData, m_fakeHandle);
            return m_fakeHandle; 
        }
    }

    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].period.containPeriod(timePeriod)) 
        {
            // Same data already in loading
            m_loading[i].awaitingHandle << m_fakeHandle++;
            return m_loading[i].awaitingHandle.last();
        }
    }

    QnTimePeriod periodToLoad = timePeriod;
    // try to reduce duration of loading period
    if (!m_loadedPeriods.isEmpty())
    {
        QnTimePeriodList::iterator itr = qUpperBound(m_loadedPeriods.begin(), m_loadedPeriods.end(), periodToLoad.startTimeMs);
        if (itr != m_loadedPeriods.begin())
            itr--;

        if (qBetween(periodToLoad.startTimeMs, itr->startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            qint64 endPoint = periodToLoad.startTimeMs + periodToLoad.durationMs;
            periodToLoad.startTimeMs = itr->startTimeMs + itr->durationMs - 20*1000; // add addition 20 sec (may server does not flush data e.t.c)
            periodToLoad.durationMs = endPoint - periodToLoad.startTimeMs;
            ++itr;
            if (itr != m_loadedPeriods.end()) {
                if (itr->startTimeMs < periodToLoad.startTimeMs + periodToLoad.durationMs)
                    periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
            }
        }
        else if (qBetween(periodToLoad.startTimeMs + periodToLoad.durationMs, itr->startTimeMs, itr->startTimeMs + itr->durationMs))
        {
            periodToLoad.durationMs = itr->startTimeMs - periodToLoad.startTimeMs;
        }
    }

    int handle = sendRequest(periodToLoad);

    m_loading << LoadingInfo(periodToLoad, handle);
    return handle;
}

void QnTimePeriodReader::at_replyReceived(int status, const QnTimePeriodList &timePeriods, int requstHandle)
{
    for (int i = 0; i < m_loading.size(); ++i)
    {
        if (m_loading[i].handle == requstHandle)
        {
            if (status == 0) {
                QVector<QnTimePeriodList> periods;
                if (!timePeriods.isEmpty() && !m_loadedData.isEmpty() && m_loadedData.last().durationMs == -1) 
                {
                    if (timePeriods.last().startTimeMs >= m_loadedData.last().startTimeMs)
                        m_loadedData.last().durationMs = 0;
                }
                periods << m_loadedData << timePeriods;
                m_loadedData = QnTimePeriod::mergeTimePeriods(periods); // union data

                periods.clear();
                QnTimePeriodList tmp;
                tmp << m_loading[i].period;
                periods << m_loadedPeriods << tmp;
                m_loadedPeriods = QnTimePeriod::mergeTimePeriods(periods); // union loaded time range info 

                // reduce right edge of loaded period info if last period under writing now
                if (!m_loadedPeriods.isEmpty() && !m_loadedData.isEmpty() && m_loadedData.last().durationMs == -1)
                {
                    qint64 lastDataTime = m_loadedData.last().startTimeMs;
                    while (!m_loadedPeriods.isEmpty() && m_loadedPeriods.last().startTimeMs > lastDataTime)
                        m_loadedPeriods.pop_back();
                    if (!m_loadedPeriods.isEmpty()) {
                        QnTimePeriod& lastPeriod = m_loadedPeriods.last();
                        lastPeriod.durationMs = qMin(lastPeriod.durationMs, lastDataTime - lastPeriod.startTimeMs);
                    }
                }

                foreach(int handle, m_loading[i].awaitingHandle)
                    emit ready(m_loadedData, handle);
                emit ready(m_loadedData, requstHandle);
            }
            else {
                foreach(int handle, m_loading[i].awaitingHandle)
                    emit failed(status, handle);
                emit failed(status, requstHandle);
            }
            m_loading.removeAt(i);
            break;
        }
    }
}

int QnTimePeriodReader::sendRequest(const QnTimePeriod& periodToLoad)
{
    return m_connection->asyncRecordedTimePeriods(
        QnNetworkResourceList() << m_resource, 
        periodToLoad.startTimeMs, 
        periodToLoad.startTimeMs + periodToLoad.durationMs, 
        1, 
        m_regions,
        this, 
        SLOT(at_replyReceived(int, const QnTimePeriodList &, int)));
}

