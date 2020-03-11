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
#include <nx/vms/metadata/metadata_helper.h>

class QnTimePeriodList;

class QnMotionHelper:
    public nx::vms::metadata::MetadataHelper
{
    Q_OBJECT
public:
    QnMotionHelper(const QString& dataDir, QObject* parent = nullptr);
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(const QnConstMetaDataV1Ptr& data);

    QnTimePeriodList matchImage(const QnChunksRequestData& request);
    QnMotionArchiveConnectionPtr createConnection(const QnResourcePtr& res, int channel);

    QnMotionArchivePtr getArchive(const QnResourcePtr& res, int channel);
    void remove(const QnResourcePtr& res);

    QnMotionHelper();
private:

    // create Find mask by region
    void createMask(const QRegion& region);
private:
    typedef QPair<QnNetworkResourcePtr, int> MotionArchiveKey;
    typedef std::map<MotionArchiveKey, QnMotionArchivePtr> MotionWriters;
    MotionWriters m_writers;
    QnMutex m_mutex;
};
