#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

struct QnFileUpload
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
Q_DECLARE_METATYPE(QnFileUpload)

class QnClientUploadWorkerPrivate;

class QnClientUploadWorker: public QObject
{
    Q_OBJECT
public:
    QnClientUploadWorker(const QnMediaServerResourcePtr& server, const QString& path, QObject* parent = nullptr);
    ~QnClientUploadWorker();

    QnFileUpload start();
    void cancel();

    QnFileUpload status() const;

signals:
    void progress(const QnFileUpload&);

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
    QScopedPointer<QnClientUploadWorkerPrivate> d;
};

