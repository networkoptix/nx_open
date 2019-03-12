#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <recorder/wearable_archive_synchronization_state.h>
#include <nx/vms/server/server_module_aware.h>
#include <utils/common/connective.h>

struct QnWearableStorageStats
{
    qint64 downloaderBytesAvailable = 0;
    qint64 downloaderBytesFree = 0;
    qint64 totalBytesAvailable = 0;
    bool haveStorages = false;
};

class QnWearableUploadManager:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnWearableUploadManager(QObject* parent);
    virtual ~QnWearableUploadManager() override;

    QnWearableStorageStats storageStats() const;

    bool consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs);

    bool isConsuming(const QnUuid& cameraId) const;
    nx::vms::server::recorder::WearableArchiveSynchronizationState state(const QnUuid& cameraId) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};
