#pragma once

#include <functional>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

#include "upload_state.h"

namespace nx::vms::client::desktop {

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
     * @param config                    Contains upload description
     * @returns                         Upload id, or null in case of an error.
     * It will write additional information to config structure.
     * UploadManager keeps a local copy of 'UploadState config' structure.
     */

    QString addUpload(
        const QnMediaServerResourcePtr& server,
        UploadState& config,
        QObject* context,
        Callback callback);

    UploadState state(const QString& id);

    void cancelUpload(const QString& id);

private:
    QHash<QString, UploadWorker*> m_workers;
};

} // namespace nx::vms::client::desktop
