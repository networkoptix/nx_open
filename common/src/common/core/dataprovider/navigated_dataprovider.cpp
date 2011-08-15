#include "navigated_dataprovider.h"

QnNavigatedDataProvider::QnNavigatedDataProvider(QnResourcePtr res):
    QnClientPullStreamProvider(res),
    m_lengthMksec(0),
    m_startMksec(0),
    m_forward(true),
    m_singleShot(false),
    //m_adaptiveSleep(20 * 1000),
    m_needToSleep(0),
    m_cs(QMutex::Recursive),
    m_useTwice(false),
    m_skipFramesToTime(0)
{

}

void QnNavigatedDataProvider::updateStreamParamsBasedOnQuality()
{

}

quint64 QnNavigatedDataProvider::skipFramesToTime() const
{
    QMutexLocker mutex(&m_framesMutex);
    return m_skipFramesToTime;
}

void QnNavigatedDataProvider::setSkipFramesToTime(quint64 skipFramesToTime)
{
    QMutexLocker mutex(&m_framesMutex);
    m_skipFramesToTime = skipFramesToTime;
}

bool QnNavigatedDataProvider::isSingleShotMode() const
{
    return m_singleShot;
}
