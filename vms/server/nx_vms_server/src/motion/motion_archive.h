#pragma once

#include <QSharedPointer>
#include "recorder/device_file_catalog.h"
#include <core/resource/resource_fwd.h>
#include "motion/abstract_motion_archive.h"

#include <motion/motion_detection.h>
#include <nx/vms/server/metadata/metadata_archive.h>

class QnMotionArchive;

class QnMotionArchiveConnection: public QnAbstractMotionArchiveConnection
{
public:
    QnAbstractCompressedMetadataPtr getMotionData(qint64 timeUsec);
    virtual ~QnMotionArchiveConnection();
private:
    QnMotionArchiveConnection(QnMotionArchive* owner);

    friend class QnMotionArchive;

private:
    QnMotionArchive* m_owner;
    qint64 m_minDate;
    qint64 m_maxDate;
    QVector<nx::vms::server::metadata::IndexRecord> m_index;
    nx::vms::server::metadata::IndexHeader m_indexHeader;
    QVector<nx::vms::server::metadata::IndexRecord>::iterator m_indexItr;
    QFile m_motionFile;

    int m_motionLoadedStart;
    int m_motionLoadedEnd;

    qint64 m_lastTimeMs;
    quint8* m_motionBuffer;
    QnMetaDataV1Ptr m_lastResult;
};

typedef std::shared_ptr<QnMotionArchiveConnection> QnMotionArchiveConnectionPtr;

class QnMotionArchive: public nx::vms::server::metadata::MetadataArchive
{
    Q_OBJECT
    using base_type = nx::vms::server::metadata::MetadataArchive;
public:
    QnMotionArchive(const QString& dataDir, const QString& uniqueId, int channel);
    virtual ~QnMotionArchive();
    QnMotionArchiveConnectionPtr createConnection();

    bool saveToArchive(QnConstMetaDataV1Ptr data);
    QnTimePeriodList matchPeriod(const Filter& filter);
private:
    friend class QnMotionArchiveConnection;
private:
    QnMetaDataV1Ptr m_lastDetailedData;
    qint64 m_lastTimestamp = 0;
};
