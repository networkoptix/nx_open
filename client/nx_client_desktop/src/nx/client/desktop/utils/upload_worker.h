#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct FileUpload
{
    enum Status
    {
        Initial,
        CalculatingMD5,
        CreatingUpload,
        Uploading,
        Checking,
        Done,
        Error,
        Canceled
    };

    /** Upload id, also serves as the filename on the server. */
    QString id;

    /** Upload size, in bytes. */
    qint64 size = 0;

    /** Total bytes uploaded. */
    qint64 uploaded = 0;

    /** Current status of the upload. */
    Status status = Initial;

    /** Error message, if any. */
    QString errorMessage;
};
Q_DECLARE_METATYPE(FileUpload)

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

    FileUpload start();
    void cancel();

    FileUpload status() const;

signals:
    void progress(const FileUpload&);

private:
    void emitProgress();
    void handleStop();
    void handleError(const QString& message);
    void handleMd5Calculated();
    void handleFileUploadCreated(bool success, rest::Handle handle);
    void handleUpload();
    void handleChunkUploaded(bool success, rest::Handle handle);
    void handleAllUploaded();
    void handleCheckFinished(bool success, rest::Handle handle, bool ok);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
