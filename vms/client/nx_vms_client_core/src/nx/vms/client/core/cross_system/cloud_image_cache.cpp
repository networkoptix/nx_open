// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_image_cache.h"

#include <cstdint>
#include <string>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>

#include <common/common_globals.h>
#include <nx/cloud/db/client/async_http_requests_executor.h>
#include <nx/network/http/buffer_source.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/buffer.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/thread_pool.h>
#include <nx/vms/client/core/network/network_manager.h>
#include <nx/vms/client/core/utils/filename_utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

namespace {

struct FileUploadRequest
{
    std::int64_t size = 0;
};
NX_REFLECTION_INSTRUMENT(FileUploadRequest, (size))

struct FileURLResponse
{
    std::string url;
    std::int64_t expireIn = 0;
};
NX_REFLECTION_INSTRUMENT(FileURLResponse, (url)(expireIn))

std::string privateFileApiPath(const QString& filename)
{
    static const QString kPrivateFilesPathTemplate = "/docdb/v1/files/private/%1";

    return nx::format(kPrivateFilesPathTemplate,
        QString::fromLatin1(QUrl::toPercentEncoding(filename))).toStdString();
}

/**
 * Reads the file in worker, avoiding blocking the caller thread. The handler receives empty data
 * if the file cannot be read.
 */
void readFileAsync(
    const QString& path,
    std::function<void(const QByteArray&)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    FileCache::ioThreadPool()->start(
        [path, handler = handlerExecutor.bind(std::move(handler))]() mutable
        {
            QFile file(path);
            handler(file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
        });
}

/**
 * Atomically writes the data in a worker, avoiding blocking the caller thread.
 */
void writeFileAsync(
    const QString& path,
    nx::Buffer data,
    std::function<void(bool)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    FileCache::ioThreadPool()->start(
        [path, data = std::move(data),
            handler = handlerExecutor.bind(std::move(handler))]() mutable
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            const bool success = file.open(QIODevice::WriteOnly)
                && file.write(data.data(), data.size()) == static_cast<qint64>(data.size())
                && file.commit();
            handler(success);
        });
}

} // namespace

struct CloudImageCache::Private
{
    nx::cloud::db::client::ApiRequestsExecutor* const apiRequestsExecutor;

    /**
     * File names of the downloads in progress, registered until the file is written to disk.
     * Removing an entry cancels the download at its next stage.
     */
    QSet<QString> downloading;
    std::unique_ptr<NetworkManager> networkManager = std::make_unique<NetworkManager>();
};

CloudImageCache::CloudImageCache(
    nx::cloud::db::client::ApiRequestsExecutor* apiRequestsExecutor,
    QObject* parent)
    :
    FileCache(parent),
    d(new Private{.apiRequestsExecutor = apiRequestsExecutor})
{
}

CloudImageCache::~CloudImageCache()
{
    d->networkManager->pleaseStopSync();
}

QString CloudImageCache::cacheFolder() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(QString("%1/cache/cloud/%2")
        .arg(root)
        .arg(Qn::kWallpapersFolder));
}

QString CloudImageCache::absoluteFilePath(const QString& filename) const
{
    const auto safeFilename = sanitizeFilename(filename);
    if (safeFilename.isEmpty())
        return {};

    return QDir::toNativeSeparators(cacheFolder() + QDir::separator() + safeFilename);
}

void CloudImageCache::clear()
{
    d->downloading.clear();
}

void CloudImageCache::downloadFile(const QString& filename)
{
    const QString safeFilename = sanitizeFilename(filename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
        return;
    }

    const QString path = absoluteFilePath(safeFilename);
    if (QFileInfo::exists(path))
        return;

    if (d->downloading.contains(safeFilename))
        return;
    d->downloading.insert(safeFilename);

    d->apiRequestsExecutor->makeAsyncCall<FileURLResponse>(
        network::http::Method::get,
        privateFileApiPath(safeFilename),
        {},
        nx::utils::AsyncHandlerExecutor(this).bind(
            [this, safeFilename, path](
                cloud::db::api::ResultCode resultCode,
                FileURLResponse urlResponse)
            {
                if (!d->downloading.contains(safeFilename))
                    return; //< The download was cancelled by deleteFile() or clear().

                if (resultCode != cloud::db::api::ResultCode::ok)
                {
                    d->downloading.remove(safeFilename);
                    NX_WARNING(this, "Failed to get download URL for file %1: %2",
                        safeFilename, resultCode);
                    emit fileDownloaded(safeFilename, OperationResult::serverError,
                        /*absolutePath*/ {});
                    return;
                }

                auto client = std::make_unique<network::http::AsyncClient>(
                    nx::network::ssl::kDefaultCertificateCheck);
                NetworkManager::setDefaultTimeouts(client.get());

                d->networkManager->doGet(std::move(client), nx::Url(urlResponse.url), this,
                    [this, safeFilename, path](NetworkManager::Response response)
                    {
                        if (!d->downloading.contains(safeFilename))
                            return; //< The download was cancelled by deleteFile() or clear().

                        if (!network::http::StatusCode::isSuccessCode(
                            response.statusLine.statusCode))
                        {
                            d->downloading.remove(safeFilename);

                            NX_WARNING(this, "Failed to download file %1 from the storage: %2",
                                safeFilename, response.statusLine.statusCode);

                            emit fileDownloaded(safeFilename, OperationResult::serverError,
                                /*absolutePath*/ {});
                            return;
                        }

                        auto onFileWritten =
                            [this, safeFilename, path](bool success)
                            {
                                if (!d->downloading.remove(safeFilename))
                                {
                                    QFile::remove(path); //< Cancelled while writing.
                                    return;
                                }

                                if (!success)
                                {
                                    NX_WARNING(this, "Failed to save cache file: %1", path);

                                    emit fileDownloaded(safeFilename, OperationResult::fileSystemError,
                                        /*absolutePath*/ {});
                                    return;
                                }

                                emit fileDownloaded(safeFilename, OperationResult::ok, path);
                            };

                        writeFileAsync(path,
                            std::move(response.messageBody),
                            std::move(onFileWritten),
                            this);
                    });
            }));
}

void CloudImageCache::uploadFile(
    const QString& filename,
    std::function<void(OperationResult)> callback)
{
    const QString safeFilename = sanitizeFilename(filename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
        if (callback)
            executeLater([callback] { callback(OperationResult::invalidOperation); }, this);
        return;
    }

    auto onFileRead =
        [this, safeFilename, callback = std::move(callback)](
            const QByteArray& fileData) mutable
        {
            if (fileData.isEmpty())
            {
                NX_WARNING(this, "File to upload is not cached locally: %1", safeFilename);
                if (callback)
                    callback(OperationResult::fileSystemError);
                return;
            }

            uploadData(safeFilename, fileData, std::move(callback));
        };

    readFileAsync(absoluteFilePath(safeFilename), std::move(onFileRead), this);
}

void CloudImageCache::uploadData(
    const QString& filename,
    QByteArray fileData,
    std::function<void(OperationResult)> callback)
{
    FileUploadRequest uploadRequest;
    uploadRequest.size = fileData.size();

    d->apiRequestsExecutor->makeAsyncCall<FileURLResponse>(
        network::http::Method::put,
        privateFileApiPath(filename),
        {},
        uploadRequest,
        nx::utils::AsyncHandlerExecutor(this).bind(
            [this, filename, fileData = std::move(fileData), callback = std::move(callback)](
                cloud::db::api::ResultCode resultCode,
                FileURLResponse urlResponse) mutable
            {
                if (resultCode != cloud::db::api::ResultCode::ok
                    && resultCode != cloud::db::api::ResultCode::created)
                {
                    NX_WARNING(this, "Failed to get upload URL for file %1: %2",
                        filename, resultCode);
                    if (callback)
                        callback(OperationResult::serverError);
                    return;
                }

                auto client = std::make_unique<network::http::AsyncClient>(
                    nx::network::ssl::kDefaultCertificateCheck);
                NetworkManager::setDefaultTimeouts(client.get());
                client->setRequestBody(std::make_unique<network::http::BufferSource>(
                    "application/octet-stream",
                    nx::Buffer(std::move(fileData))));

                d->networkManager->doPut(
                    std::move(client),
                    nx::Url(urlResponse.url),
                    this,
                    [this, filename, callback = std::move(callback)](
                        NetworkManager::Response response)
                    {
                        const bool ok = network::http::StatusCode::isSuccessCode(
                            response.statusLine.statusCode);
                        if (!ok)
                        {
                            NX_WARNING(this, "Failed to upload file %1 to the storage: %2",
                                filename, response.statusLine.statusCode);
                        }

                        if (callback)
                            callback(ok ? OperationResult::ok : OperationResult::serverError);
                    });
            }));
}

void CloudImageCache::deleteFile(const QString& filename)
{
    const auto safeFilename = sanitizeFilename(filename);
    if (safeFilename.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", filename);
        return;
    }

    if (d->downloading.remove(safeFilename))
    {
        executeLater(
            [this, safeFilename]
            {
                emit fileDownloaded(safeFilename, OperationResult::invalidOperation,
                    /*absolutePath*/ {});
            },
            this);
    }

    d->apiRequestsExecutor->makeAsyncCall<void>(
        network::http::Method::delete_,
        privateFileApiPath(safeFilename),
        /*urlQuery*/ {},
        nx::utils::AsyncHandlerExecutor(this).bind(
            [this, safeFilename](cloud::db::api::ResultCode resultCode)
            {
                if (resultCode != cloud::db::api::ResultCode::ok)
                {
                    NX_WARNING(this, "Failed to delete file %1: %2", safeFilename, resultCode);
                    return;
                }

                const auto path = absoluteFilePath(safeFilename);
                if (QFile::exists(path) && !QFile::remove(path))
                    NX_WARNING(this, "Failed to remove the cached file: %1", path);
            }));
}

} // namespace nx::vms::client::core
