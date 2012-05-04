#ifndef QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
#define QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H

#include <QMap>
#include <QMutex>
#include "time_period_loader.h"

/**
 * Time period loader that can be used to load recorded time periods for several
 * cameras.
 */
class QnMultiCameraTimePeriodLoader: public QObject {
    Q_OBJECT
public:
    QnMultiCameraTimePeriodLoader(QnNetworkResourcePtr resource, QObject *parent);

    static QnMultiCameraTimePeriodLoader* newInstance(QnResourcePtr resource, QObject *parent = 0);

    int load(const QnTimePeriod &period, const QList<QRegion> &motionRegions = QList<QRegion>());
    QnNetworkResourcePtr resource() const;

signals:
    void ready(const QnTimePeriodList &timePeriods, int handle);
    void failed(int, int handle);

private slots:
    void onDataLoaded(const QnTimePeriodList &periods, int handle);
    void onLoadingFailed(int code, int handle);

private:
    int loadInternal(QnNetworkResourcePtr networkResource, const QnTimePeriod &period, const QList<QRegion> &motionRegions);

private:
    QMutex m_mutex;
    QMap<QnNetworkResourcePtr, QnTimePeriodLoader *> m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, QVector<QnTimePeriodList> > m_multiLoadPeriod;
    QnNetworkResourcePtr m_resource;
};

#endif // QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
