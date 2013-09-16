#include "motion_helper.h"
#include "motion_archive.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "utils/common/util.h"
#include <QFileInfo>
#include <QDir>
#include "recorder/file_deletor.h"
#include "serverutil.h"


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

QnMotionArchive* QnMotionHelper::getArchive(QnResourcePtr res, int channel)
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

void QnMotionHelper::saveToArchive(QnMetaDataV1Ptr data)
{
    QnMotionArchive* archive = getArchive(data->dataProvider->getResource(), data->channelNumber);
    if (archive)
        archive->saveToArchive(data);

}

QnMotionArchiveConnectionPtr QnMotionHelper::createConnection(QnResourcePtr res, int channel)
{
    QnMotionArchive* archive = getArchive(res, channel);
    if (archive) 
        return archive->createConnection();
    else
        return QnMotionArchiveConnectionPtr();
}

QnTimePeriodList QnMotionHelper::mathImage(const QList<QRegion>& regions, QnResourcePtr res, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QVector<QnTimePeriodList> data;
    mathImage( regions, res, msStartTime, msEndTime, detailLevel, &data );
    return QnTimePeriod::mergeTimePeriods(data);
}

QnTimePeriodList QnMotionHelper::mathImage(const QList<QRegion>& regions, QnResourceList resList, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    QVector<QnTimePeriodList> data;
    foreach(QnResourcePtr res, resList)
        mathImage( regions, res, msStartTime, msEndTime, detailLevel, &data );
    //NOTE could just call prev method instead of private one, but that will result in multiple QnTimePeriod::mergeTimePeriods calls, which could worsen performance
    return QnTimePeriod::mergeTimePeriods(data);
}

void QnMotionHelper::mathImage(
    const QList<QRegion>& regions,
    QnResourcePtr res,
    qint64 msStartTime,
    qint64 msEndTime,
    int detailLevel,
    QVector<QnTimePeriodList>* const timePeriods )
{
    for (int i = 0; i < regions.size(); ++i)
    {
        QnSecurityCamResourcePtr securityCamRes = res.dynamicCast<QnSecurityCamResource>();
        if( securityCamRes && securityCamRes->isDtsBased() )
        {
            *timePeriods << securityCamRes->getDtsTimePeriodsByMotionRegion( regions, msStartTime, msEndTime, detailLevel );
        }
        else
        {
            QnMotionArchive* archive = getArchive(res, i);
            if (archive) 
                *timePeriods << archive->mathPeriod(regions[i], msStartTime, msEndTime, detailLevel);
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

QString QnMotionHelper::getBaseDir(const QString& macAddress)
{
#ifdef _TEST_TWO_SERVERS
    return closeDirPath(getDataDirectory()) + QString("test/record_catalog/metadata/") + macAddress + QString("/");
#else
    return closeDirPath(getDataDirectory()) + QString("record_catalog/metadata/") + macAddress + QString("/");
#endif
}

QString QnMotionHelper::getMotionDir(const QDate& date, const QString& macAddress)
{
    return getBaseDir(macAddress) + date.toString("yyyy/MM/");
}

QList<QDate> QnMotionHelper::recordedMonth(const QString& macAddress)
{
    QList<QDate> rez;
    QDir baseDir(getBaseDir(macAddress));
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

void QnMotionHelper::deleteUnusedFiles(const QList<QDate>& monthList, const QString& macAddress)
{
    QList<QDate> existsData = recordedMonth(macAddress);
    foreach(const QDate& date, existsData) {
        if (!monthList.contains(date))
            qnFileDeletor->deleteDir(getMotionDir(date, macAddress));
    }
}
