#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>
#include <QtCore/QMap>
#include "utils/media/sse_helper.h"
#include "recorder/device_file_catalog.h"
#include "motion_archive.h"
#include "core/resource/resource_fwd.h"
#include <nx/utils/singleton.h>
#include <nx/mediaserver/server_module_aware.h>

class QnTimePeriodList;

class QnMotionHelper: public QObject
{
    Q_OBJECT
public:
    QnMotionHelper(const QString& dataDir, QObject* parent = nullptr);
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(const QnConstMetaDataV1Ptr& data);

    QnTimePeriodList matchImage(const QList<QRegion>& region, const QnResourceList& resList, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnTimePeriodList matchImage(const QList<QRegion>& region, const QnResourcePtr& res, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnMotionArchiveConnectionPtr createConnection(const QnResourcePtr& res, int channel);

    QnMotionArchive* getArchive(const QnResourcePtr& res, int channel);

    QString getMotionDir(const QDate& date, const QString& cameraUniqueId) const;
    void deleteUnusedFiles(const QList<QDate>& chunks, const QString& cameraUniqueId) const;
    QList<QDate> recordedMonth(const QString& cameraUniqueId) const;

    QnMotionHelper();
    QString getBaseDir() const;
private:
    QString getBaseDir(const QString& cameraUniqueId) const;

    // create Find mask by region
    void createMask(const QRegion& region);

    // match one motion image by mask
    void matchImage(
        const QList<QRegion>& regions,
        const QnResourcePtr& res,
        qint64 msStartTime,
        qint64 msEndTime,
        int detailLevel,
        std::vector<QnTimePeriodList>* const timePeriods );

private:
    typedef QPair<QnNetworkResourcePtr, int> MotionArchiveKey;
    typedef QMap<MotionArchiveKey, QnMotionArchive*> MotionWriters;
    MotionWriters m_writers;
    QnMutex m_mutex;
    const QString m_dataDir;
};
