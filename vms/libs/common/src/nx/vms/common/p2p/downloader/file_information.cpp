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

float FileInformation::calculateDownloadProgress() const
{
    int size = downloadedChunks.size();
    if (!size)
        return 0;
    int done = downloadedChunks.count(true);
    return 100.0*done / size;
}

int FileInformation::calculateDownloadedBytes() const
{
    int done = downloadedChunks.count(true);
    return done * chunkSize;
}

QString FileInformation::key() const
{
    return keyFromFileName(name);
}

QString FileInformation::keyFromFileName(const QString& fileName)
{
    return QFileInfo(fileName).fileName();
}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, Status)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, PeerSelectionPolicy)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileInformation, (json)(eq), FileInformation_Fields, (optional, true)(brief, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
