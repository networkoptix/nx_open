// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_file_info.h"
#include "layout_file_info_v1.h"

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

std::optional<FileInfo> parseVerson2(QFile&& file)
{
    FileInfo info;
    Footer footer;
    if (!file.seek(file.size() - sizeof(footer)))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, too small size", file.fileName());
        return std::nullopt;
    }

    if (file.read((char*)&footer, sizeof(footer)) != sizeof(footer))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read footer", file.fileName());
        return std::nullopt;
    }

    if (footer.magic != Footer::kIndexMagic)
    {
        NX_WARNING(NX_SCOPE_TAG, "Corrupted NOV file: %1, invalid header magic: %2",
            file.fileName(), footer.magic);
        return std::nullopt;
    }

    if (footer.version != Footer::kCurrentVersion)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported NOV version, file: %1, verison: %2",
            file.fileName(), footer.version);
        return std::nullopt;
    }

    if (!file.seek(footer.offset))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read index, offset: %2",
            file.fileName(), footer.offset);
        return std::nullopt;
    }

    Header header;
    if (file.read((char*)&header, sizeof(header)) != sizeof(header))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read header", file.fileName());
        return std::nullopt;
    }

    if (header.hasCryptoData)
    {
        CryptoInfo crypto;
        if (file.read((char*)&crypto, sizeof(crypto)) != sizeof(crypto))
        {
            NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read crypto info",
                file.fileName());
            return std::nullopt;
        }
        info.cryptoInfo = crypto;
    }

    for (uint32_t i = 0; i < header.entryCount; ++i)
    {
        StreamIndexEntry entry;
        if (file.read((char*)&entry, sizeof(entry)) != sizeof(entry))
        {
            NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read index: %2",
                file.fileName(), i);
            return std::nullopt;
        }
        info.entries.push_back(entry);
    }
    return info;
}

bool writeNovFileIndex(const QString& fileName, const FileInfo& info)
{

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;

    Footer footer;
    footer.offset = file.size();

    Header header;
    header.hasCryptoData = info.cryptoInfo.has_value() ? 1 : 0;
    header.entryCount = info.entries.size();
    file.write((const char*)&header, sizeof(header));

    if (info.cryptoInfo)
        file.write((const char*)&(*info.cryptoInfo), sizeof(CryptoInfo));

    for(const auto& entry: info.entries)
        file.write((const char*)&entry, sizeof(entry));

    file.write((const char*)&footer, sizeof(footer));

    if (file.error() != QFile::NoError)
        return false;

    return true;
}

std::optional<FileInfo> readNovFileIndex(const QString& fileName, bool allowTemp)
{
    const QString extension = QFileInfo(fileName).suffix().toLower();
    if (!(extension == "nov" || extension == "exe"
        || (allowTemp && (fileName.endsWith(".exe.tmp") || fileName.endsWith(".nov.tmp")))))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid NOV file extension: %1", extension);
        return std::nullopt;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to open NOV file: %1", fileName);
        return std::nullopt;
    }

    // Check if this is new version of NOV file.
    Footer footer;
    if (!file.seek(file.size() - sizeof(footer)))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file, failed to read header, %1", fileName);
        return std::nullopt;
    }
    if (file.read((char*)&footer, sizeof(footer)) != sizeof(footer))
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid nov file: %1, failed to read header", fileName);
        return std::nullopt;
    }
    file.seek(0);

    std::optional<FileInfo> info = (footer.magic == Footer::kIndexMagic)
        ? parseVerson2(std::move(file))
        : parseVerson1(std::move(file), extension == "exe");

    if (info)
        NX_DEBUG(NX_SCOPE_TAG, "Parsed NOV file: %1", fileName);

    return info;
}

int FileInfo::totalIndexHeaderSize()
{
    return sizeof(Header)
        + (cryptoInfo ? sizeof(CryptoInfo) : 0)
        + entries.size() * sizeof(StreamIndexEntry)
        + sizeof(Footer);
}

bool checkPassword(const QString& password, const CryptoInfo& cryptoInfo)
{
    return nx::crypt::checkSaltedPassword(password,
        cryptoInfo.passwordSalt, cryptoInfo.passwordHash);
}

} // namespace nx::core::layout
