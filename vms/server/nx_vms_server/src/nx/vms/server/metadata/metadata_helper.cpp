#include "metadata_helper.h"
#include <utils/common/util.h>

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

} // namespace nx::vms::server::metadata
