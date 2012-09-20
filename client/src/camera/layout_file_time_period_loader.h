#ifndef QN_LAYOUT_FILE_TIME_PERIOD_LOADER_H
#define QN_LAYOUT_FILE_TIME_PERIOD_LOADER_H


#include <QtCore/QMap>
#include <QtCore/QMutex>

#include "time_period_loader.h"
#include "abstract_time_period_loader.h"

/**
 * Time period loader that can be used to load recorded time periods from exported layout
 * cameras.
 */

class QnLayoutFileTimePeriodLoader: public QnAbstractTimePeriodLoader
{
public:
    QnLayoutFileTimePeriodLoader(QnResourcePtr resource, QObject *parent, const QnTimePeriodList& chunks);
    static QnLayoutFileTimePeriodLoader* newInstance(QnResourcePtr resource, QObject *parent = 0);
    virtual int load(const QnTimePeriod &period, const QList<QRegion> &motionRegions = QList<QRegion>()) override;
signals:

private:
    QnTimePeriodList m_chunks;
    int m_handle;
};

#endif // QN_LAYOUT_FILE_TIME_PERIOD_LOADER_H
