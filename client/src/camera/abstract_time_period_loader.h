#ifndef __QN_ABSTRACT_TIME_PERIOD_LOADER_H__
#define __QN_ABSTRACT_TIME_PERIOD_LOADER_H__

#include <QObject>
#include <QRegion>
#include <QList>
#include "recording/time_period_list.h"
#include <core/resource/resource.h>

class QnAbstractTimePeriodLoader: public QObject
{
    Q_OBJECT
public:
    QnAbstractTimePeriodLoader(QnResourcePtr resource, QObject *parent);
    virtual ~QnAbstractTimePeriodLoader() {}

    /**
     * \param timePeriod                Time period to get motion periods for.
     * \param motionRegions             List of regions to look for motion, one region per video channel.
     *                                  If empty list is provided, then recorded time periods will be returned.
     * \returns                         Request handle.
     * 
     * \note                            This function is thread-safe.
     */
    virtual int load(const QnTimePeriod &period, const QList<QRegion> &motionRegions = QList<QRegion>()) = 0;

    /**
     * \returns                         Network resource representing the camera that this
     *                                  loader works with.
     * 
     * \note                            This function is thread-safe.
     */
    QnResourcePtr resource() const { return m_resource; }

signals:
    /**
     * This signal is emitted whenever motion periods were successfully loaded from server.
     *
     * \param timePeriods               Loaded motion periods.
     * \param handle                    Request handle.
     */
    void ready(const QnTimePeriodList &timePeriods, int handle);

    /**
     * This signal is emitted whenever the reader was unable to load motion periods from server.
     * 
     * \param status                    Error code.
     * \param handle                    Request handle.
     */
    void failed(int status, int handle);

protected:
    /** Network resource that this loader gets chunks for. */
    QnResourcePtr m_resource;
signals:
    void delayedReady(const QnTimePeriodList &timePeriods, int handle);
};

#endif // __QN_ABSTRACT_TIME_PERIOD_LOADER_H__
