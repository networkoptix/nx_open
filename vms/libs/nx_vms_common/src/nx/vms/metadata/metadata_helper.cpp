// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_helper.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QRegion>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/media/motion_detection.h>
#include <nx/vms/common/serialization/qt_gui_types.h>
#include <utils/common/util.h>

namespace nx::vms::metadata {

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

QString MetadataHelper::dataDir() const
{
    return m_dataDir;
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

void MetadataHelper::deleteUnusedFiles(
    const QList<QDate>& monthList, const QString& cameraUniqueId) const
{
    QList<QDate> existsData = recordedMonth(cameraUniqueId);
    for (const QDate& date : existsData) {
        if (!monthList.contains(date))
            cleanupMetadataDir(getDirByDateTime(date, cameraUniqueId));
    }
}

QList<QRegion> MetadataHelper::regionsFromFilter(const QString& filter, int channelCount)
{
    if (filter.isEmpty())
        return QList<QRegion>();
    return QJson::deserialized<QList<QRegion>>(filter.toUtf8());
}

} // namespace nx::vms::metadata
