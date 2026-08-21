// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "filename_utils.h"

#include <QtCore/QFileInfo>

namespace nx::vms::client::core {

bool isFilenameSafe(const QString& unsafeFilename)
{
    return !sanitizeFilename(unsafeFilename).isEmpty();
}

QString sanitizeFilename(const QString& unsafeFilename)
{
    // The last component of the path, which is expected to be the filename.
    // It can be empty if the path ends with a separator or is empty itself.
    QString filename = QFileInfo(unsafeFilename).fileName();

    if (filename.isEmpty()
        || filename == "."
        || filename == ".."
        || filename != unsafeFilename)
    {
        return {};
    }

    // Windows (NTFS) strips trailing whitespace and dots when resolving filenames, so `"foo "`
    // and `"foo."` collide with `"foo"`. Reject such names to keep cache keys unique.
    const QChar last = filename.back();
    if (last == '.' || last.isSpace())
        return {};

    return filename;
}

} // namespace nx::vms::client::core
