#include <QRegExp>
#include <nx/utils/log/assert.h>
#include "manual_file_data.h"

namespace nx {
namespace update {
namespace info {

ManualFileData::ManualFileData(const QString& file, const OsVersion& osVersion,
    const nx::utils::SoftwareVersion& nxVersion, bool isClient)
    :
    file(file),
    osVersion(osVersion),
    nxVersion(nxVersion),
    isClient(isClient)
{}

ManualFileData ManualFileData::fromFileName(const QString& fileName)
{
    const QRegExp fileRegExp("^.+-([a-z]+)_update-([0-9:.]+)-([0-9,a-z]+)-?.*\\.zip$");
    ManualFileData result;

    if (int pos = fileRegExp.indexIn(fileName); pos == -1)
        return ManualFileData();

    const auto capture1 = fileRegExp.cap(1);
    NX_ASSERT(capture1 == "server" || capture1 == "client");
    result.isClient = capture1 == "client";
    result.nxVersion = nx::utils::SoftwareVersion(fileRegExp.cap(2));
    result.osVersion = OsVersion::fromString(fileRegExp.cap(3));
    result.file = fileName;

    return result;
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
