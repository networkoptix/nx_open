#include "motion_helper.h"
#include "motion_archive.h"
#include <nx/streaming/abstract_stream_data_provider.h>
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
    QnMutexLocker lock( &m_mutex );
    for(QnMotionArchive* writer: m_writers.values())
        delete writer;
    m_writers.clear();
}

QnMotionArchive* QnMotionHelper::getArchive(const QnResourcePtr& res, int channel)
{
    QnMutexLocker lock( &m_mutex );
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
    std::vector<QnTimePeriodList> data;
    matchImage( regions, res, msStartTime, msEndTime, detailLevel, &data );
    return QnTimePeriodList::mergeTimePeriods(data);
}

QnTimePeriodList QnMotionHelper::matchImage(const QList<QRegion>& regions, const QnResourceList& resList, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    std::vector<QnTimePeriodList> data;
    for(const QnResourcePtr& res: resList)
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
    std::vector<QnTimePeriodList>* const timePeriods )
{
    for (int i = 0; i < regions.size(); ++i)
    {
        QnSecurityCamResource* securityCamRes = dynamic_cast<QnSecurityCamResource*>(res.data());
        if( securityCamRes && securityCamRes->isDtsBased() )
        {
            timePeriods->push_back(securityCamRes->getDtsTimePeriodsByMotionRegion( regions, msStartTime, msEndTime, detailLevel ));
        }
        else
        {
            QnMotionArchive* archive = getArchive(res, i);
            if (archive)
                timePeriods->push_back(archive->matchPeriod(regions[i], msStartTime, msEndTime, detailLevel));
        }
    }
}

QString QnMotionHelper::getBaseDir(const QString& cameraUniqueId)
{
    QString base = closeDirPath(getDataDirectory());
    QString separator = getPathSeparator(base);
    return base + QString("record_catalog%1metadata%2").arg(separator).arg(separator) + cameraUniqueId + separator;
}

QString QnMotionHelper::getBaseDir()
{
    QString base = closeDirPath(getDataDirectory());
    QString separator = getPathSeparator(base);
    return base + QString("record_catalog%1metadata").arg(separator);
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
    for(const QFileInfo& fiYear: yearList)
    {
        QDir yearDir(fiYear.absoluteFilePath());
        QList<QFileInfo> monthDirs = yearDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
        for(const QFileInfo& fiMonth: monthDirs)
        {
            QDir monthDir(fiMonth.absoluteFilePath());
            if (!monthDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files).isEmpty())
                rez << QDate(fiYear.baseName().toInt(), fiMonth.baseName().toInt(), 1);
        }
    }
    return rez;
}

void cleanupMotionDir(const QString& _dirName)
{
    QString dirName = QDir::toNativeSeparators(_dirName);
    if (dirName.endsWith(QDir::separator()))
        dirName.chop(1);

    QDir dir(dirName);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
        QFile::remove(fi.absoluteFilePath());
    for(int i = 0; i < 3; ++i)
    {
        QDir dir (dirName);
        if (!dir.rmdir(dirName))
            break;
        dirName = dirName.left(dirName.lastIndexOf(QDir::separator()));
    }
}

void QnMotionHelper::deleteUnusedFiles(const QList<QDate>& monthList, const QString& cameraUniqueId)
{
    QList<QDate> existsData = recordedMonth(cameraUniqueId);
    for(const QDate& date: existsData) {
        if (!monthList.contains(date))
            cleanupMotionDir(getMotionDir(date, cameraUniqueId));
    }
}
