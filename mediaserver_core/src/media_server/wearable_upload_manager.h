#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/connective.h>
#include <recorder/wearable_archive_synchronization_state.h>

class QnWearableUploadManager: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWearableUploadManager(QObject* parent = nullptr);
    virtual ~QnWearableUploadManager() override;

    bool consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs);

    bool isConsuming(const QnUuid& cameraId) const;
    nx::mediaserver_core::recorder::WearableArchiveSynchronizationState state(const QnUuid& cameraId) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};
