#ifndef QN_CACHING_TIME_PERIOD_LOADER_H
#define QN_CACHING_TIME_PERIOD_LOADER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>


class QnTimePeriodLoader;
class QnAbstractTimePeriodLoader;

class QnCachingTimePeriodLoader: public QObject {
    Q_OBJECT;
    Q_PROPERTY(qreal loadingMargin READ loadingMargin WRITE setLoadingMargin);

public:
    QnCachingTimePeriodLoader(const QnResourcePtr &networkResource, QObject *parent = NULL);
    virtual ~QnCachingTimePeriodLoader();

    static QnCachingTimePeriodLoader *newInstance(const QnResourcePtr &resource, QObject *parent = NULL);

    QnResourcePtr resource();

    qreal loadingMargin() const;
    void setLoadingMargin(qreal loadingMargin);

    qint64 updateInterval() const;
    void setUpdateInterval(qint64 msecs);

    const QnTimePeriod &loadedPeriod() const;
    void setTargetPeriods(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod);
    
    const QList<QRegion> &motionRegions() const;
    void setMotionRegions(const QList<QRegion> &motionRegions);
    bool isMotionRegionsEmpty() const;

    QnTimePeriodList periods(Qn::TimePeriodRole type);

signals:
    void periodsChanged(Qn::TimePeriodRole type);
    void loadingFailed();

private slots:
    void at_loader_ready(const QnTimePeriodList &timePeriods, int handle);
    void at_loader_failed(int status, int handle);

protected:
    void load(Qn::TimePeriodRole type);
    void trim(Qn::TimePeriodRole type, qint64 trimTime);

    QnTimePeriod addLoadingMargins(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod) const;

private:
    QnCachingTimePeriodLoader(QnAbstractTimePeriodLoader **loaders, QObject *parent);

    void init();
    void initLoaders(QnAbstractTimePeriodLoader **loaders);
    static bool createLoaders(const QnResourcePtr &resource, QnAbstractTimePeriodLoader **loaders);

private:
    QnResourcePtr m_resource;
    QnTimePeriod m_loadedPeriod;
    QnAbstractTimePeriodLoader *m_loaders[Qn::TimePeriodRoleCount];
    int m_handles[Qn::TimePeriodRoleCount];
    QList<QRegion> m_motionRegions;
    QnTimePeriodList m_periods[Qn::TimePeriodRoleCount];
    qreal m_loadingMargin;
    qint64 m_updateInterval;
};


#endif // QN_CACHING_TIME_PERIOD_LOADER_H
