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
    QnLayoutFileTimePeriodLoader(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, const QnTimePeriodList& chunks, QObject *parent);
    virtual ~QnLayoutFileTimePeriodLoader();
    static QnLayoutFileTimePeriodLoader* newInstance(const QnResourcePtr &resource, Qn::TimePeriodContent periodsType, QObject *parent = 0);
    virtual int load(const QnTimePeriod &period, const QString &filter) override;
private:
    int loadChunks(const QnTimePeriod &period);
    int loadMotion(const QnTimePeriod &period, const QList<QRegion> &motionRegions);
private:
    QnTimePeriodList m_chunks;
    //QnMetaDataV1Light* m_motionData;
    //int m_motionDataSize;
    static int m_handle;
};

#endif // QN_LAYOUT_FILE_TIME_PERIOD_LOADER_H
