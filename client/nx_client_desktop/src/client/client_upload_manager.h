#pragma once

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

#include "client_upload_worker.h"

class QnClientUploadManager: public QObject
{
    Q_OBJECT
public:
    QnClientUploadManager(QObject* parent = nullptr);
    ~QnClientUploadManager();

    /**
     * Adds a file to the upload queue.
     *
     * Note that as only one file can be uploaded at a time, the upload might
     * not start right away.
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
    QHash<QString, QnClientUploadWorker*> m_workers;
};

