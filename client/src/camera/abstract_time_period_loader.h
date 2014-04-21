#ifndef __QN_ABSTRACT_TIME_PERIOD_LOADER_H__
#define __QN_ABSTRACT_TIME_PERIOD_LOADER_H__

#include <QtCore/QObject>
#include <QtGui/QRegion>
#include <QtCore/QList>
#include "recording/time_period_list.h"
#include <core/resource/resource_fwd.h>

class QnAbstractTimePeriodLoader: public QObject
{
    Q_OBJECT
public:
    QnAbstractTimePeriodLoader(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent);
    virtual ~QnAbstractTimePeriodLoader() {}

    /**
     * \param timePeriod                Time period to get motion periods for.
     * \param motionRegions             List of regions to look for motion, one region per video channel.
     *                                  If empty list is provided, then recorded time periods will be returned.
     * \returns                         Request handle.
     * 
     * \note                            This function is thread-safe.
     */
    virtual int load(const QnTimePeriod &period, const QString &filter) = 0;

    /**
     * \returns                         Resource that this loader works with.
     * 
     * \note                            This function is thread-safe.
     */
    QnResourcePtr resource() const { return m_resource; }

    /**
     * Discards cached data, if any.
     * 
     * \note                            This function is thread-safe.
     */
    Q_SLOT virtual void discardCachedData() {}

signals:
    /**
     * This signal is emitted whenever motion periods were successfully loaded.
     *
     * \param timePeriods               Loaded motion periods.
     * \param handle                    Request handle.
     */
    void ready(const QnTimePeriodList &timePeriods, int handle);

    /**
     * This signal is emitted whenever the reader was unable to load motion periods.
     * 
     * \param status                    Error code.
     * \param handle                    Request handle.
     */
    void failed(int status, int handle);

protected:
    /** Resource that this loader gets chunks for. */
    const QnResourcePtr m_resource;

    /** Periods type this loader reading. */
    const Qn::TimePeriodContent m_periodsType;
signals:
    void delayedReady(const QnTimePeriodList &timePeriods, int handle);
};

#endif // __QN_ABSTRACT_TIME_PERIOD_LOADER_H__
