#pragma once

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

struct QnFileUpload
{
    enum Status
    {
        Opened,
        CalculatingMD5,
        CreatingUpload,
        Uploading,
        Checking,
        Done,
        Error
    };

    /** Upload id, also serves as the filename on the server. */
    QString id;

    /** Upload size, in bytes. */
    qint64 size = 0;

    /** Total bytes uploaded. */
    qint64 uploaded = 0;

    /** Current status of the upload. */
    Status status = Error;

    /** Error message, if any. */
    QString errorMessage;
};
Q_DECLARE_METATYPE(QnFileUpload)

class QnFileUploadData;

class QnClientUploadManager: public QObject
{
    Q_OBJECT
public:
    QnClientUploadManager(QObject* parent = nullptr);
    ~QnClientUploadManager();

    /**
     * Adds a file to the upload queue.
     *
     * Note that as only one file can be uploaded at a time, the upload might not start right away.
     *
     * @param server                    Mediaserver to upload to.
     * @param path                      Path to the file.
     * @returns                         Upload description. Don't forget to check for
     *                                  errors in the return value.
     */
    QnFileUpload addUpload(const QnMediaServerResourcePtr& server, const QString& path);

signals:
    void progress(const QnFileUpload&);

private:
    QnFileUploadData* currentData() const;
    QnFileUploadData* dropCurrentData();
    void handleError(const QString& message);
    void handleStarted();
    void handleMd5Calculated();
    void handleFileUploadCreated(bool success, rest::Handle handle);
    void handleUpload();
    void handleChunkUploaded(bool success, rest::Handle handle);
    void handleAllUploaded();
    void handleCheckFinished(bool success, rest::Handle handle, bool ok);

    void advance();

private:
    QList<QnFileUploadData*> m_dataList;
};

