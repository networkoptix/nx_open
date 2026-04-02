// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_file_info_v1.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/utils/crypt/crypted_file_stream.h>
#include <nx/utils/log/log.h>

namespace nx::core::layout {

std::optional<FileInfo> parseVerson1(QFile&& file, const bool isExe)
{
    FileInfo info;
    if (isExe)
    {
        file.seek(file.size() - 2 * sizeof(qint64));
        quint64 magic;
        quint64 offset;
        file.read((char*)&offset, sizeof(qint64));
        file.read((char*)&magic, sizeof(qint64));
        if (magic != StreamIndexV1::kFileMagic)
        {
            NX_WARNING(NX_SCOPE_TAG, "Corrupted NOV file: %1, invalid magic: %2",
                file.fileName(), magic);
            return std::nullopt;
        }

        file.seek(offset);
    }

    StreamIndexV1 index;
    if (file.read((char *)&index, sizeof(index)) != sizeof(index))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to read NOV file: %1", file.fileName());
        return std::nullopt;
    }

    if (index.magic != StreamIndexV1::kIndexCryptedMagic
        && index.magic != StreamIndexV1::kIndexMagic1)
    {
        NX_WARNING(NX_SCOPE_TAG, "Corrupted NOV file: %1, invalid index magic: %2",
            file.fileName(), index.magic);
        return std::nullopt;
    }

    if (index.version != 1)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported file version: %1(%2)",
            index.version, file.fileName());
        return std::nullopt;
    }

    if (index.entryCount > StreamIndexV1::kMaxStreams)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Failed to read NOV file: %1, invalid entry count: %1",
            file.fileName(), index.entryCount);
        return std::nullopt;
    }

    info.entries.resize(index.entryCount);
    std::copy(index.entries.begin(), index.entries.begin() + index.entryCount, info.entries.begin());

    if (index.magic == StreamIndexV1::kIndexCryptedMagic)
    {
        CryptoInfo crypto;
        if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to read NOV file: %1", file.fileName());
            return std::nullopt;
        }
        info.cryptoInfo = crypto;
    }
    return info;
}

} // namespace nx::core::layout
