#ifndef QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
#define QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H

#include <QMap>
#include <QMutex>
#include "time_period_loader.h"

/**
 * Time period loader that can be used to load recorded time periods for several
 * cameras.
 */
class QnMultiCameraTimePeriodLoader: public QObject
{
    Q_OBJECT
public:
    QnMultiCameraTimePeriodLoader();

    static QnMultiCameraTimePeriodLoader *instance();

    int load(const QnNetworkResourceList &networkResources, const QnTimePeriod &period);

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
    typedef QMap<QnNetworkResourcePtr, QnTimePeriodLoaderPtr> NetResCache;
    NetResCache m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, QVector<QnTimePeriodList> > m_multiLoadPeriod;
    int m_multiRequestCount;
};

#endif // QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
