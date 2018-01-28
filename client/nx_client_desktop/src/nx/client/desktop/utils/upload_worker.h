#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include "upload_state.h"

namespace nx {
namespace client {
namespace desktop {

class UploadWorker: public QObject
{
    Q_OBJECT

public:
    UploadWorker(
        const QnMediaServerResourcePtr& server,
        const QString& path,
        qint64 ttl,
        QObject* parent = nullptr);
    virtual ~UploadWorker() override;

    UploadState start();
    void cancel();

    UploadState status() const;

signals:
    void progress(const UploadState&);

private:
    void emitProgress();
    void handleStop();
    void handleError(const QString& message);
    void handleMd5Calculated();
    void handleFileUploadCreated(bool success);
    void handleUpload();
    void handleChunkUploaded(bool success);
    void handleAllUploaded();
    void handleCheckFinished(bool success, bool ok);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

