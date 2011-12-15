#include "motion_helper.h"
#include "motion_archive.h"
#include "core/dataprovider/abstract_streamdataprovider.h"

QnMotionHelper::QnMotionHelper()
{
}

QnMotionHelper::~QnMotionHelper()
{
    QMutexLocker lock(&m_mutex);
    foreach(QnMotionArchive* writer, m_writers.values())
        delete writer;
    m_writers.clear();
}

QnMotionArchive* QnMotionHelper::getArchive(QnResourcePtr res)
{
    QMutexLocker lock(&m_mutex);
    QnNetworkResourcePtr netres = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (!netres)
        return 0;
    QnMotionArchive* writer = m_writers.value(netres);
    if (writer == 0) {
        writer = new QnMotionArchive(netres);
        m_writers.insert(netres, writer);
    }
    return writer;
}

void QnMotionHelper::saveToArchive(QnMetaDataV1Ptr data)
{
    QnMotionArchive* archive = getArchive(data->dataProvider->getResource());
    if (archive)
        archive->saveToArchive(data);

}

void QnMotionHelper::maskMotion(QnMetaDataV1Ptr data)
{
    QnMotionArchive* archive = getArchive(data->dataProvider->getResource());
    if (archive)
        archive->maskMotion(data);
}

QnTimePeriodList QnMotionHelper::mathImage(const QRegion& region, QnResourcePtr res, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QnTimePeriodList rez;
    QnMotionArchive* archive = getArchive(res);
    if (archive) 
        rez =  archive->mathPeriod(region, msStartTime, msEndTime, detailLevel);
    return rez;
}

QnMotionArchiveConnectionPtr QnMotionHelper::createConnection(QnResourcePtr res)
{
    QnMotionArchive* archive = getArchive(res);
    if (archive) 
        return archive->createConnection();
    else
        return QnMotionArchiveConnectionPtr();
}


QnTimePeriodList QnMotionHelper::mathImage(const QRegion& region, QnResourceList resList, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QVector<QnTimePeriodList> data;
    foreach(QnResourcePtr res, resList)
    {
        QnMotionArchive* archive = getArchive(res);
        if (archive) 
            data << archive->mathPeriod(region, msStartTime, msEndTime, detailLevel);
    }
    return QnTimePeriod::mergeTimePeriods(data);
}

Q_GLOBAL_STATIC(QnMotionHelper, inst);

QnMotionHelper* QnMotionHelper::instance()
{
    return inst();
}
