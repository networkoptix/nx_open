// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_information.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

FileInformation::FileInformation(const QString& fileName):
    name(fileName)
{
}

bool FileInformation::isValid() const
{
    return !name.isEmpty();
}

int FileInformation::downloadProgress() const
{
    return 100 * downloadedBytes() / size;
}

qint64 FileInformation::downloadedBytes() const
{
    qint64 result = downloadedChunks.count(true) * chunkSize;
    if (downloadedChunks.testBit(downloadedChunks.size() - 1))
        result -= (chunkSize - size % chunkSize);
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileInformation, (json), FileInformation_Fields, (optional, true)(brief, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
