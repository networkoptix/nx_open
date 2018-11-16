#include "motion_helper.h"
#include "motion_archive.h"
#include <nx/streaming/abstract_stream_data_provider.h>
#include "utils/common/util.h"
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include "recorder/file_deletor.h"

#include <core/resource/camera_resource.h>
#include <recording/time_period_list.h>


QnMotionHelper::QnMotionHelper(const QString& dataDir, QObject* parent):
    QObject(parent),
    m_dataDir(dataDir)
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
        writer = new QnMotionArchive(m_dataDir, netres, channel);
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

QnTimePeriodList QnMotionHelper::matchImage(const QnChunksRequestData& request)
{
    const auto motionRegionList = request.filter.trimmed().isEmpty()
        ? QList<QRegion>()
        : QJson::deserialized<QList<QRegion>>(request.filter.toUtf8());

    const auto wholeFrameRegionList =
        [](int channelCount)
        {
            static const QRegion kWholeFrame(
                QRect(0, 0, Qn::kMotionGridWidth, Qn::kMotionGridHeight));

            QList<QRegion> result;
            result.reserve(channelCount);
            std::fill_n(std::back_inserter(result), channelCount, kWholeFrame);
            return result;
        };

    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& res: request.resList)
    {
        const auto motionRegions = motionRegionList.isEmpty()
            ? wholeFrameRegionList(res->getVideoLayout()->channelCount())
            : motionRegionList;

        if (res->isDtsBased())
        {
            timePeriods.push_back(res->getDtsTimePeriodsByMotionRegion(
                motionRegions,
                request.startTimeMs,
                request.endTimeMs,
                request.detailLevel.count(),
                request.keepSmallChunks,
                request.limit,
                request.sortOrder));
        }
        else
        {
            for (int i = 0; i < motionRegions.size(); ++i)
            {
                QnMotionArchive* archive = getArchive(res, i);
                if (archive)
                {
                    timePeriods.push_back(archive->matchPeriod(
                        motionRegions[i],
                        request.startTimeMs,
                        request.endTimeMs,
                        request.detailLevel.count(),
                        request.limit,
                        request.sortOrder));
                }
            }
        }
    }
    return QnTimePeriodList::mergeTimePeriods(timePeriods, request.limit, request.sortOrder);
}

QString QnMotionHelper::getBaseDir(const QString& cameraUniqueId) const
{
    QString base = closeDirPath(m_dataDir);
    QString separator = getPathSeparator(base);
    return base + QString("record_catalog%1metadata%2").arg(separator).arg(separator) + cameraUniqueId + separator;
}

QString QnMotionHelper::getBaseDir() const
{
    QString base = closeDirPath(m_dataDir);
    QString separator = getPathSeparator(base);
    return base + QString("record_catalog%1metadata").arg(separator);
}

QString QnMotionHelper::getMotionDir(const QDate& date, const QString& cameraUniqueId) const
{
    return getBaseDir(cameraUniqueId) + date.toString("yyyy/MM/");
}

QList<QDate> QnMotionHelper::recordedMonth(const QString& cameraUniqueId) const
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

void QnMotionHelper::deleteUnusedFiles(const QList<QDate>& monthList, const QString& cameraUniqueId) const
{
    QList<QDate> existsData = recordedMonth(cameraUniqueId);
    for(const QDate& date: existsData) {
        if (!monthList.contains(date))
            cleanupMotionDir(getMotionDir(date, cameraUniqueId));
    }
}
