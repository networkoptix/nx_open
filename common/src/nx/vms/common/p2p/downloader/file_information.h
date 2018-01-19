#pragma once

#include <QtCore/QUrl>
#include <QtCore/QBitArray>

#include <nx/fusion/model_functions_fwd.h>

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

    QString name;
    qint64 size = -1;
    QByteArray md5;
    QUrl url;
    qint64 chunkSize = 0;
    Status status = Status::notFound;
    QBitArray downloadedChunks;
    qint64 touchTime = 0;
    qint64 ttl = 0;
};
#define FileInformation_Fields \
    (name)(size)(md5)(url)(chunkSize)(status)(downloadedChunks)(touchTime)(ttl)

QN_FUSION_DECLARE_FUNCTIONS(FileInformation, (json))
QN_FUSION_DECLARE_FUNCTIONS(FileInformation::Status, (lexical))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
