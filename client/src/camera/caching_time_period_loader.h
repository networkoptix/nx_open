#ifndef QN_CACHING_TIME_PERIOD_LOADER_H
#define QN_CACHING_TIME_PERIOD_LOADER_H

#include <QtCore/QObject>

#include <recording/time_period.h>

class QnTimePeriodLoader;

class QnCachingTimePeriodLoader: public QObject {
public:
    QnCachingTimePeriodLoader(QObject *parent = NULL);
    virtual ~QnCachingTimePeriodLoader();

    QnTimePeriodLoader *loader();
    void setLoader(QnTimePeriodLoader *loader);

    qreal loadingMargin() const;
    void setLoadingMargin(qreal loadingMargin);

    const QnTimePeriod &loadedPeriod() const;

    QnTimePeriodList periods(const QnTimePeriod &targetPeriod, const QList<QRegion> &motionRegions = QList<QRegion>());

signals:
    void periodsChanged(Qn::TimePeriodType type);
    void loadingFailed();

private slots:
    void at_loader_ready(const QnTimePeriodList &timePeriods, int handle);
    void at_loader_failed(int status, int handle);
    void at_loader_destroyed();

protected:
    void load(Qn::TimePeriodType type);

    QnTimePeriod addLoadingMargins(const QnTimePeriod &targetPeriod) const;

private:
    QnTimePeriodLoader *m_loader;
    QnTimePeriod m_loadedPeriod;
    int m_handles[Qn::TimePeriodTypeCount];
    QList<QRegion> m_motionRegions;
    QnTimePeriodList m_periods[Qn::TimePeriodTypeCount];
    qreal m_loadingMargin;
};


#endif // QN_CACHING_TIME_PERIOD_LOADER_H
