#ifndef QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
#define QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H

#include <QtCore/QMap>
#include <QtCore/QMutex>

#include "time_period_loader.h"
#include "abstract_time_period_loader.h"

/**
 * Time period loader that can be used to load recorded time periods for several
 * cameras.
 */

class QnMultiCameraTimePeriodLoader: public QnAbstractTimePeriodLoader
{
    Q_OBJECT
public:
    QnMultiCameraTimePeriodLoader(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent = 0);
    static QnMultiCameraTimePeriodLoader *newInstance(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent = 0);

    virtual int load(const QnTimePeriod &period, const QString &filter) override;

    virtual void discardCachedData() override;

private slots:
    void onDataLoaded(const QnTimePeriodList &periods, int handle);
    void onLoadingFailed(int code, int handle);

private:
    int loadInternal(QnMediaServerResourcePtr mServer, QnNetworkResourcePtr networkResource, const QnTimePeriod &period, const QString &filter);

private:
    QMutex m_mutex;
    //QMap<QnNetworkResourcePtr, QnTimePeriodLoader *> m_cache;
    QMap<QString, QnTimePeriodLoader *> m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, QVector<QnTimePeriodList> > m_multiLoadPeriod;
};

#endif // QN_MULTI_CAMERA_TIME_PERIOD_LOADER_H
