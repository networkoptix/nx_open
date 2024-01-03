// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

/**%apidoc
 * Data stored by the Server for the file previously registered in the Server's Downloader
 * via POST /api/downloads/{fileName}.
 */
struct NX_VMS_COMMON_API FileInformation
{
    Q_GADGET

public:
    FileInformation(const QString& fileName = QString());

    bool isValid() const;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status,
        /**%apidoc There's no such file in the downloader. */
        notFound = 0,

        /**%apidoc The file is being downloaded from an external URL and Site Servers. */
        downloading,

        /**%apidoc Waiting for the file to be manually uploaded. */
        uploading,

        /**%apidoc The file is stored on the Server entirely. */
        downloaded,

        /**%apidoc The file has been downloaded, but the checksum validation has failed. */
        corrupted
    )

    NX_REFLECTION_ENUM_IN_CLASS(PeerSelectionPolicy,
        none, /**<%apidoc Only the provided url will be used for downloading. */
        all,
        byPlatform
    )

    /**%apidoc Id of the file record, supplied to API calls as {fileName}. */
    QString name;

    /**%apidoc Size of the file, in bytes. */
    qint64 size = -1;

    /**%apidoc MD5 checksum of the entire file. */
    QByteArray md5;

    /**%apidoc URL via which the file could be downloaded from the Internet. */
    nx::utils::Url url;

    /**%apidoc Size of a chunk, in bytes. */
    qint64 chunkSize = 0;

    /**%apidoc Current state of the file record. */
    Status status = Status::notFound;

    /**%apidoc
     * Bit mask, where each bit represents the respective chunk of the file and shows whether the
     * Server has the chunk content. The bit mask is represented as an array of bytes encoded in
     * Base64. The first chunk (with index 0) corresponds to the least significant bit of the first
     * byte (with index 0), and the last chunk corresponds to the most significant bit of the last
     * byte.
     */
    QBitArray downloadedChunks;

    /**%apidoc[proprietary] */
    PeerSelectionPolicy peerPolicy = PeerSelectionPolicy::none;

    /**%apidoc
     * Time, in milliseconds since epoch, when the file was last written to. Used to determine
     * whether the file needs to be deleted when its time-to-live (specified in "ttl" field) has
     * passed since the moment defined by "touchTime" field.
     **/
    qint64 touchTime = 0;

    /**%apidoc
     * Time-to-live of the file record, in milliseconds. After this period is expired since the
     * moment defined by "touchTime" field, the file record and the file content is deleted. 0
     * means the record will not be automatically deleted.
     */
    qint64 ttl = 0;

    /**%apidoc[proprietary] */
    QList<QnUuid> additionalPeers;

    /**%apidoc
     * Path to the file's parent directory on the Server. If empty, the default path is used.
     */
    QString absoluteDirectoryPath;

    /**%apidoc[proprietary] */
    QString fullFilePath;

    /**%apidoc[proprietary] Opaque user data*/
    QString userData;

    /** Calculates a progress for download, [0..100]. */
    int downloadProgress() const;

    /** Calculates total number of bytes downloaded. */
    qint64 downloadedBytes() const;
};

#define FileInformation_Fields \
    (name) \
    (size) \
    (md5) \
    (url) \
    (chunkSize) \
    (status) \
    (downloadedChunks) \
    (peerPolicy) \
    (touchTime) \
    (ttl) \
    (additionalPeers) \
    (absoluteDirectoryPath) \
    (fullFilePath) \
    (userData)

QN_FUSION_DECLARE_FUNCTIONS(FileInformation, (json), NX_VMS_COMMON_API)

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
