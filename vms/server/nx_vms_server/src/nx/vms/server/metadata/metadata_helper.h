#pragma once

#include <QtCore/QObject>
#include <QtCore/QDate>

namespace nx::vms::server::metadata {

static const int kMetadataIndexHeaderSize = 16;
static const int kIndexRecordSize = 8;

class MetadataHelper : public QObject
{
public:
    MetadataHelper(const QString& dataDir, QObject* parent = nullptr);
    QString getDirByDateTime(const QDate& date, const QString& cameraUniqueId) const;
    QString getBaseDir() const;
    QList<QDate> recordedMonth(const QString& cameraUniqueId) const;
    void deleteUnusedFiles(const QList<QDate>& monthList, const QString& cameraUniqueId) const;
protected:
    QString getBaseDir(const QString& cameraUniqueId) const;
    QList<QRegion> regionsFromFilter(const QString& filter, int channelCount) const;
protected:
    const QString m_dataDir;
};

} // namespace nx::vms::server::metadata
