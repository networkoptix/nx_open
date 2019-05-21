#include "metadata_helper.h"

#include <QtGui/QRegion>

#include <utils/common/util.h>
#include <motion/motion_detection.h>

namespace nx::vms::server::metadata {

MetadataHelper::MetadataHelper(const QString& dataDir, QObject* parent) :
    QObject(parent),
    m_dataDir(dataDir)
{

}

QString MetadataHelper::getDirByDateTime(const QDate& date, const QString& cameraUniqueId) const
{
    return getBaseDir(cameraUniqueId) + date.toString("yyyy/MM/");
}

QString MetadataHelper::getBaseDir(const QString& cameraUniqueId) const
{
    QString base = closeDirPath(m_dataDir);
    QString separator = getPathSeparator(base);
    return base + QString("metadata%1").arg(separator) + cameraUniqueId + separator;
}

QString MetadataHelper::getBaseDir() const
{
    QString base = closeDirPath(m_dataDir);
    QString separator = getPathSeparator(base);
    return base + QString("metadata");
}

QList<QDate> MetadataHelper::recordedMonth(const QString& cameraUniqueId) const
{
    QList<QDate> rez;
    QDir baseDir(getBaseDir(cameraUniqueId));
    QList<QFileInfo> yearList = baseDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
    for (const QFileInfo& fiYear : yearList)
    {
        QDir yearDir(fiYear.absoluteFilePath());
        QList<QFileInfo> monthDirs = yearDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
        for (const QFileInfo& fiMonth : monthDirs)
        {
            QDir monthDir(fiMonth.absoluteFilePath());
            if (!monthDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files).isEmpty())
                rez << QDate(fiYear.baseName().toInt(), fiMonth.baseName().toInt(), 1);
        }
    }
    return rez;
}

void cleanupMetadataDir(const QString& _dirName)
{
    QString dirName = QDir::toNativeSeparators(_dirName);
    if (dirName.endsWith(QDir::separator()))
        dirName.chop(1);

    QDir dir(dirName);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : list)
        QFile::remove(fi.absoluteFilePath());
    for (int i = 0; i < 3; ++i)
    {
        QDir dir(dirName);
        if (!dir.rmdir(dirName))
            break;
        dirName = dirName.left(dirName.lastIndexOf(QDir::separator()));
    }
}

void MetadataHelper::deleteUnusedFiles(const QList<QDate>& monthList, const QString& cameraUniqueId) const
{
    QList<QDate> existsData = recordedMonth(cameraUniqueId);
    for (const QDate& date : existsData) {
        if (!monthList.contains(date))
            cleanupMetadataDir(getDirByDateTime(date, cameraUniqueId));
    }
}

QList<QRegion> MetadataHelper::regionsFromFilter(const QString& filter, int channelCount) const
{
    const auto motionRegionList = filter.trimmed().isEmpty()
        ? QList<QRegion>()
        : QJson::deserialized<QList<QRegion>>(filter.toUtf8());

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

    return motionRegionList.isEmpty()
        ? wholeFrameRegionList(channelCount)
        : motionRegionList;
}

} // namespace nx::vms::server::metadata
