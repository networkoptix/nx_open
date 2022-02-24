// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QDate>

namespace nx::vms::metadata {

class NX_VMS_COMMON_API MetadataHelper: public QObject
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

} // namespace nx::vms::metadata
