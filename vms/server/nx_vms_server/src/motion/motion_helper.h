#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>
#include <QtCore/QMap>
#include "utils/media/sse_helper.h"
#include "recorder/device_file_catalog.h"
#include "motion_archive.h"
#include "core/resource/resource_fwd.h"
#include <nx/vms/server/server_module_aware.h>
#include <api/helpers/chunks_request_data.h>

class QnTimePeriodList;

class QnMotionHelper: public QObject
{
    Q_OBJECT
public:
    QnMotionHelper(const QString& dataDir, QObject* parent = nullptr);
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(const QnConstMetaDataV1Ptr& data);

    QnTimePeriodList matchImage(const QnChunksRequestData& request);
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

private:
    typedef QPair<QnNetworkResourcePtr, int> MotionArchiveKey;
    typedef QMap<MotionArchiveKey, QnMotionArchive*> MotionWriters;
    MotionWriters m_writers;
    QnMutex m_mutex;
    const QString m_dataDir;
};
