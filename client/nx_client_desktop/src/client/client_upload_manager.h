#pragma once

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

struct QnFileUpload {
    enum Status {
        Opened,
        CalculatingMD5,
        CreatingUpload,
        Uploading,
        Checking,
        Done,
        Error
    };

    QString id;
    qint64 size = 0;
    qint64 uploaded = 0;
    Status status = Error;
    QString message;
};
Q_DECLARE_METATYPE(QnFileUpload)

class QnFileUploadData;

class QnClientUploadManager: public QObject {
    Q_OBJECT
public:
    QnClientUploadManager(QObject* parent = nullptr);
    ~QnClientUploadManager();

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

