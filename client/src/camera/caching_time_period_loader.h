#ifndef QN_CACHING_TIME_PERIOD_LOADER_H
#define QN_CACHING_TIME_PERIOD_LOADER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <recording/time_period.h>

class QnTimePeriodLoader;

class QnCachingTimePeriodLoader: public QObject {
    Q_OBJECT;
    Q_PROPERTY(qreal loadingMargin READ loadingMargin WRITE setLoadingMargin);

public:
    QnCachingTimePeriodLoader(QObject *parent = NULL);
    virtual ~QnCachingTimePeriodLoader();

    static QnCachingTimePeriodLoader *newInstance(const QnResourcePtr &resouce, QObject *parent = NULL);

    QnTimePeriodLoader *loader();
    void setLoader(QnTimePeriodLoader *loader);

    qreal loadingMargin() const;
    void setLoadingMargin(qreal loadingMargin);

    qint64 updateInterval() const;
    void setUpdateInterval(qint64 msecs);

    const QnTimePeriod &loadedPeriod() const;
    void setTargetPeriod(const QnTimePeriod &targetPeriod);
    
    const QList<QRegion> &motionRegions() const;
    void setMotionRegions(const QList<QRegion> &motionRegions);

    QnTimePeriodList periods(Qn::TimePeriodType type);

signals:
    void periodsChanged(Qn::TimePeriodType type);
    void loadingFailed();

private slots:
    void at_loader_ready(const QnTimePeriodList &timePeriods, int handle);
    void at_loader_failed(int status, int handle);
    void at_loader_destroyed();

protected:
    void load(Qn::TimePeriodType type);
    void trim(Qn::TimePeriodType type, qint64 trimTime);

    QnTimePeriod addLoadingMargins(const QnTimePeriod &targetPeriod) const;

private:
    QnTimePeriodLoader *m_loader;
    QnTimePeriod m_loadedPeriod;
    int m_handles[Qn::TimePeriodTypeCount];
    QList<QRegion> m_motionRegions;
    QnTimePeriodList m_periods[Qn::TimePeriodTypeCount];
    qreal m_loadingMargin;
    qint64 m_updateInterval;
};


#endif // QN_CACHING_TIME_PERIOD_LOADER_H
