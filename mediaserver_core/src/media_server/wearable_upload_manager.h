#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/connective.h>

class QnWearableUploadManager: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWearableUploadManager(QObject* parent = nullptr);
    virtual ~QnWearableUploadManager() override;

    bool consume(const QnUuid& cameraId, const QnUuid& token, const QString& uploadId, qint64 startTimeMs);

    bool isConsuming(const QnUuid& cameraId);

private:
    QnMutex m_mutex;
    QSet<QnUuid> m_consuming;
};
