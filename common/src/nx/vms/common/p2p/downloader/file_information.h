#pragma once

#include <QtCore/QUrl>
#include <QtCore/QBitArray>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/url.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

struct FileInformation
{
    Q_GADGET

public:
    FileInformation(const QString& fileName = QString());

    bool isValid() const;

    enum class Status
    {
        /** There's no such file in the downloader. */
        notFound,
        /** The file is downloading from an external URL and system servers. */
        downloading,
        /** Waiting for the file to be manually uploaded. */
        uploading,
        /** The file is downloaded. */
        downloaded,
        /** The file has been downloaded, but checksum validation has failed. */
        corrupted
    };
    Q_ENUM(Status)

    enum PeerSelectionPolicy
    {
        none, //< only provided url will be used for downloading
        all = 1 << 0,
        byPlatform = 1 << 1,
    };
    Q_ENUM(PeerSelectionPolicy)

    QString name;
    qint64 size = -1;
    QByteArray md5;
    nx::utils::Url url;
    qint64 chunkSize = 0;
    Status status = Status::notFound;
    QBitArray downloadedChunks;
    PeerSelectionPolicy peerPolicy = PeerSelectionPolicy::none;
    qint64 touchTime = 0;
    qint64 ttl = 0;
};
#define FileInformation_Fields \
    (name)(size)(md5)(url)(chunkSize)(status)(downloadedChunks)(peerPolicy)(touchTime)(ttl)

QN_FUSION_DECLARE_FUNCTIONS(FileInformation, (json))
QN_FUSION_DECLARE_FUNCTIONS(FileInformation::Status, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(FileInformation::PeerSelectionPolicy, (lexical))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::common::p2p::downloader::FileInformation)
