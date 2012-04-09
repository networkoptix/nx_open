#ifndef QN_TIME_PERIOD_READER_HELPER_H
#define QN_TIME_PERIOD_READER_HELPER_H

#include <QMap>
#include <QMutex>
#include "time_period_reader.h"

/**
 *  
 */
class QnTimePeriodReaderHelper: public QObject
{
    Q_OBJECT
public:
    QnTimePeriodReaderHelper();

    static QnTimePeriodReaderHelper *instance();

    QnTimePeriodReaderPtr createUpdater(QnResourcePtr resource);
    int load(const QnNetworkResourceList &netResList, const QnTimePeriod &period);

signals:
    void ready(const QnTimePeriodList &timePeriods, int handle);
    void failed(int, int handle);

private slots:
    void onDataLoaded(const QnTimePeriodList &periods, int handle);
    void onLoadingFailed(int code, int handle);

private:
    int load(QnNetworkResourcePtr netRes, const QnTimePeriod &period);

private:
    QMutex m_mutex;
    typedef QMap<QnNetworkResourcePtr, QnTimePeriodReaderPtr> NetResCache;
    NetResCache m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, QVector<QnTimePeriodList> > m_multiLoadPeriod;
    int m_multiRequestCount;
};

#endif // QN_TIME_PERIOD_READER_HELPER_H
