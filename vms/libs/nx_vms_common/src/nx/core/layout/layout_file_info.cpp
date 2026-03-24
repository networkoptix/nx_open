// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_file_info.h"

#include <exception>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <nx/utils/crypt/crypted_file_stream.h>
#include <nx/utils/log/log.h>

namespace nx::core::layout {

bool isLayoutExtension(const QString& fileName)
{
    const QString extension = QFileInfo(fileName).suffix().toLower();
    return extension == "nov" || extension == "exe";
}

std::optional<FileInfo> identifyFile(const QString& fileName, bool allowTemp)
{
    const QString extension = QFileInfo(fileName).suffix().toLower();
    if (!(extension == "nov" || extension == "exe"
        || (allowTemp && (fileName.endsWith(".exe.tmp") || fileName.endsWith(".nov.tmp")))))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invlaid NOV file extension: %1", extension);
        return std::nullopt;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to open NOV file: %1", fileName);
        return std::nullopt;
    }

    FileInfo info;
    if (extension == "exe")
    {
        file.seek(file.size() - 2 * sizeof(qint64));
        quint64 magic;
        quint64 offset;
        file.read((char*)&offset, sizeof(qint64));
        file.read((char*)&magic, sizeof(qint64));
        if (magic != kFileMagic)
        {
            NX_WARNING(NX_SCOPE_TAG, "Corrupted NOV file, invalid magic: %1", magic);
            return std::nullopt;
        }

        file.seek(offset);
    }

    StreamIndex1 index;
    if (file.read((char *)&index, sizeof(index)) != sizeof(index))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to read NOV file: %1", fileName);
        return std::nullopt;
    }

    info.entries.resize(index.entryCount);
    std::copy(index.entries.begin(), index.entries.begin() + index.entryCount, info.entries.begin());
    info.version = index.version;

    if (index.magic == StreamIndex1::kIndexCryptedMagic)
    {
        CryptoInfo crypto;
        if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
        {
            NX_WARNING(NX_SCOPE_TAG, "Failed to read NOV file: %1", fileName);
            return std::nullopt;
        }
        info.cryptoInfo = crypto;
    }
    else if (index.magic != StreamIndex1::kIndexMagic1)
    {
        NX_WARNING(NX_SCOPE_TAG, "Corrupted NOV file, invalid index magic: %1", index.magic);
        return std::nullopt;
    }

    if (info.version > kMaxVersion)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported file from the future version: %1", info.version);
        return std::nullopt;
    }
    NX_DEBUG(NX_SCOPE_TAG, "Parsed NOV file: %1", fileName);
    return info;
}

bool checkPassword(const QString& password, const CryptoInfo& cryptoInfo)
{
    return nx::crypt::checkSaltedPassword(password,
        cryptoInfo.passwordSalt, cryptoInfo.passwordHash);
}

} // namespace nx::core::layout
