// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/file_cache/file_cache.h>

namespace nx::cloud::db::client { class ApiRequestsExecutor; }

namespace nx::vms::client::core {

/**
 * Image cache backed by the cloud private files API and a dedicated local folder, isolated from
 * the local wallpapers cache. File transfers go directly between the client and the cloud
 * storage: the API only authorizes the operation and issues a temporary storage URL.
 * Results are always reported asynchronously in the thread of this object: the fileDownloaded
 * signal for downloads, the completion callback for uploads.
 */
class NX_VMS_CLIENT_CORE_API CloudImageCache: public FileCache
{
    Q_OBJECT

public:
    /** The executor must outlive the cache and stop its requests before destruction. */
    explicit CloudImageCache(
        nx::cloud::db::client::ApiRequestsExecutor* apiRequestsExecutor,
        QObject* parent = nullptr);
    virtual ~CloudImageCache() override;

    virtual QString absoluteFilePath(const QString& filename) const override;
    virtual void clear() override;

    /**
     * Download the cloud file into the cache folder. Emits fileDownloaded on completion, silently
     * does nothing when the file is already cached, being downloaded, or the filename is unsafe.
     * @param filename Filename as stored in the layout data, values rejected by
     *     sanitizeFilename() are ignored with a warning.
     */
    void downloadFile(const QString& filename);

    /**
     * Upload a file from the cache folder to the cloud. The callback is invoked with
     * `serverError` on a transfer failure, `fileSystemError` when the file is not cached
     * locally, and `invalidOperation` for an unsafe filename.
     * @param filename Filename as stored in the layout data, values rejected by
     *     sanitizeFilename() are ignored with a warning.
     */
    void uploadFile(const QString& filename, std::function<void(OperationResult)> callback = {});

    /**
     * Delete the file from the cloud, dropping the locally cached copy on success.
     * @param filename Filename as stored in the layout data, values rejected by
     *     sanitizeFilename() are ignored with a warning.
     */
    void deleteFile(const QString& filename);

signals:
    void fileDownloaded(
        const QString& filename, OperationResult status, const QString& absolutePath);

protected:
    virtual QString cacheFolder() const override;

private:
    void uploadData(const QString& filename,
        QByteArray fileData,
        std::function<void(OperationResult)> callback);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
