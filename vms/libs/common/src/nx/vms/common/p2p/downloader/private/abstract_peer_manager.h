#pragma once

#include <functional>
#include <optional>
#include <future>
#include <memory>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <api/server_rest_connection_fwd.h>

#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class AbstractPeerManager: public QObject
{
public:
    enum Capability
    {
        FileInfo = 1 << 0,
        Checksums = 1 << 1,
        DownloadChunk = 1 << 2,
        AllCapabilities = FileInfo | Checksums | DownloadChunk
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    AbstractPeerManager(Capabilities capabilities, QObject* parent = nullptr):
        QObject(parent),
        capabilities(capabilities)
    {
    }

    virtual ~AbstractPeerManager() = default;

    template<typename T>
    struct RequestContext
    {
        using Result = std::optional<T>;
        using Future = std::future<Result>;
        Future future;
        std::function<void()> cancelFunction = {};

        void cancel() const
        {
            if (cancelFunction)
                cancelFunction();
        }

        bool isValid() const
        {
            return future.valid();
        }
    };

    // All peer managers managers share promise between handler and canceller, and it is hard to
    // guarantee a certain call order. This function simplifies their code.
    template<typename T>
    static void setPromiseValueIfEmpty(
        const std::shared_ptr<std::promise<T>>& promise, const T& value)
    {
        try
        {
            promise->set_value(value);
        }
        catch (const std::future_error&)
        {
        }
    }

    /** @return Human readable peer name. This is mostly used in logs. */
    virtual QString peerString(const QnUuid& peerId) const { return peerId.toString(); }
    virtual QList<QnUuid> getAllPeers() const = 0;
    virtual QList<QnUuid> peers() const = 0;
    virtual int distanceTo(const QnUuid& peerId) const = 0;

    virtual RequestContext<FileInformation> requestFileInfo(
        const QnUuid& peer, const QString& fileName, const nx::utils::Url& url) = 0;

    virtual RequestContext<QVector<QByteArray>> requestChecksums(
        const QnUuid& peer, const QString& fileName) = 0;

    virtual RequestContext<QByteArray> downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize) = 0;

    const Capabilities capabilities;
};

} // namespace nx::vms::common::p2p::downloader
