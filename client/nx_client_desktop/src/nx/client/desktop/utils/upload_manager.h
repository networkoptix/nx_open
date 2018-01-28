#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

#include "upload_state.h"

namespace nx {
namespace client {
namespace desktop {

class UploadWorker;

class UploadManager: public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const UploadState&)>;

    UploadManager(QObject* parent = nullptr);
    virtual ~UploadManager() override;

    /**
     * Adds a file to the upload queue.
     *
     * Note that as only one file can be uploaded at a time, the upload might
     * not start right away.
     *
     * @param server                    Mediaserver to upload to.
     * @param path                      Path to the file.
     * @param ttl                       TTL for the file, in milliseconds. File will be deleted
     *                                  from the server when TTL passes. -1 means infinite.
     * @returns                         Upload description. Don't forget to check for
     *                                  errors in the return value.
     */
    UploadState addUpload(
        const QnMediaServerResourcePtr& server,
        const QString& path,
        qint64 ttl,
        QObject* context,
        Callback callback);

    void cancelUpload(const QString& id);

private:
    QHash<QString, UploadWorker*> m_workers;
};

} // namespace desktop
} // namespace client
} // namespace nx
