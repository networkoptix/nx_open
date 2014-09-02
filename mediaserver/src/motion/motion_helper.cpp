#include "motion_helper.h"
#include "motion_archive.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "utils/common/util.h"
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include "recorder/file_deletor.h"

#include <core/resource/security_cam_resource.h>

#include <media_server/serverutil.h>

#include <recording/time_period_list.h>


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

QnMotionArchive* QnMotionHelper::getArchive(const QnResourcePtr& res, int channel)
{
    QMutexLocker lock(&m_mutex);
    QnNetworkResourcePtr netres = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (!netres)
        return 0;
    QnMotionArchive* writer = m_writers.value(MotionArchiveKey(netres, channel));
    if (writer == 0) {
        writer = new QnMotionArchive(netres, channel);
        m_writers.insert(MotionArchiveKey(netres, channel), writer);
    }
    return writer;
}

void QnMotionHelper::saveToArchive(const QnConstMetaDataV1Ptr& data)
{
    QnMotionArchive* archive = getArchive(data->dataProvider->getResource(), data->channelNumber);
    if (archive)
        archive->saveToArchive(data);

}

QnMotionArchiveConnectionPtr QnMotionHelper::createConnection(const QnResourcePtr& res, int channel)
{
    QnMotionArchive* archive = getArchive(res, channel);
    if (archive) 
        return archive->createConnection();
    else
        return QnMotionArchiveConnectionPtr();
}

QnTimePeriodList QnMotionHelper::matchImage(const QList<QRegion>& regions, const QnResourcePtr& res, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QVector<QnTimePeriodList> data;
    matchImage( regions, res, msStartTime, msEndTime, detailLevel, &data );
    return QnTimePeriodList::mergeTimePeriods(data);
}

QnTimePeriodList QnMotionHelper::matchImage(const QList<QRegion>& regions, const QnResourceList& resList, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QVector<QnTimePeriodList> data;
    foreach(QnResourcePtr res, resList)
        matchImage( regions, res, msStartTime, msEndTime, detailLevel, &data );
    //NOTE could just call prev method instead of private one, but that will result in multiple QnTimePeriodList::mergeTimePeriods calls, which could worsen performance
    return QnTimePeriodList::mergeTimePeriods(data);
}

void QnMotionHelper::matchImage(
    const QList<QRegion>& regions,
    const QnResourcePtr& res,
    qint64 msStartTime,
    qint64 msEndTime,
    int detailLevel,
    QVector<QnTimePeriodList>* const timePeriods )
{
    for (int i = 0; i < regions.size(); ++i)
    {
        QnSecurityCamResource* securityCamRes = dynamic_cast<QnSecurityCamResource*>(res.data());
        if( securityCamRes && securityCamRes->isDtsBased() )
        {
            *timePeriods << securityCamRes->getDtsTimePeriodsByMotionRegion( regions, msStartTime, msEndTime, detailLevel );
        }
        else
        {
            QnMotionArchive* archive = getArchive(res, i);
            if (archive) 
                *timePeriods << archive->matchPeriod(regions[i], msStartTime, msEndTime, detailLevel);
        }
    }
}

//Q_GLOBAL_STATIC(QnMotionHelper, inst);
static QnMotionHelper* globalInstance = NULL;

void QnMotionHelper::initStaticInstance( QnMotionHelper* inst )
{
    globalInstance = inst;
}

QnMotionHelper* QnMotionHelper::instance()
{
    return globalInstance;
    //return inst();
}

QString QnMotionHelper::getBaseDir(const QString& cameraUniqueId)
{
#ifdef _TEST_TWO_SERVERS
    return closeDirPath(getDataDirectory()) + QString("test/record_catalog/metadata/") + cameraUniqueId + L'/';
#else
    return closeDirPath(getDataDirectory()) + QString("record_catalog/metadata/") + cameraUniqueId + L'/';
#endif
}

QString QnMotionHelper::getMotionDir(const QDate& date, const QString& cameraUniqueId)
{
    return getBaseDir(cameraUniqueId) + date.toString("yyyy/MM/");
}

QList<QDate> QnMotionHelper::recordedMonth(const QString& cameraUniqueId)
{
    QList<QDate> rez;
    QDir baseDir(getBaseDir(cameraUniqueId));
    QList<QFileInfo> yearList = baseDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
    foreach(const QFileInfo& fiYear, yearList)
    {
        QDir yearDir(fiYear.absoluteFilePath());
        QList<QFileInfo> monthDirs = yearDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
        foreach(const QFileInfo& fiMonth, monthDirs)
        {
            QDir monthDir(fiMonth.absoluteFilePath());
            if (!monthDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files).isEmpty())
                rez << QDate(fiYear.baseName().toInt(), fiMonth.baseName().toInt(), 1);
            else 
                monthDir.remove(fiMonth.absoluteFilePath());
        }
    }
    return rez;
}

void QnMotionHelper::deleteUnusedFiles(const QList<QDate>& monthList, const QString& cameraUniqueId)
{
    QList<QDate> existsData = recordedMonth(cameraUniqueId);
    foreach(const QDate& date, existsData) {
        if (!monthList.contains(date))
            qnFileDeletor->deleteDir(getMotionDir(date, cameraUniqueId));
    }
}
