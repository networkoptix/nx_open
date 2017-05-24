#pragma once

#include <QtCore/QUrl>
#include <QtCore/QBitArray>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

struct FileInformation
{
    Q_GADGET

public:
    FileInformation();
    FileInformation(const QString& fileName);

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
};
#define FileInformation_Fields \
    (name)(size)(md5)(url)(chunkSize)(status)(downloadedChunks)

QN_FUSION_DECLARE_FUNCTIONS(FileInformation, (json))
QN_FUSION_DECLARE_FUNCTIONS(FileInformation::Status, (lexical))

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
