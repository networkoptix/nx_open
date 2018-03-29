#include <QRegExp>
#include "manual_file_data.h"

namespace nx {
namespace update {
namespace info {

ManualFileData::ManualFileData(const QString& file, const OsVersion& osVersion,
    const QnSoftwareVersion& nxVersion, bool isClient)
    :
    file(file),
    osVersion(osVersion),
    nxVersion(nxVersion),
    isClient(isClient)
{}

ManualFileData ManualFileData::fromFileName(const QString& fileName)
{
    const QRegExp fileRegExp("^.+-([a-z]+)_update_([0-9:.]+)-(.+)\.zip$");
    return ManualFileData();
}

bool ManualFileData::isNull() const
{
    return file.isNull();
}

bool operator==(const ManualFileData& lhs, const ManualFileData& rhs)
{
    return lhs.file == rhs.file;
}

} // namespace info
} // namespace update
} // namespace nx
