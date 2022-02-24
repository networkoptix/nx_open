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

int FileInformation::calculateDownloadProgress() const
{
    int size = downloadedChunks.size();
    if (size <= 0)
        return 0;

    int done = downloadedChunks.count(true);
    return 100 * done / size;
}

qint64 FileInformation::calculateDownloadedBytes() const
{
    int done = downloadedChunks.count(true);
    return done * chunkSize;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileInformation, (json), FileInformation_Fields, (optional, true)(brief, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
