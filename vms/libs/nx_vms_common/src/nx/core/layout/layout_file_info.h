// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <array>

#include <QtCore/QtGlobal>

// This module defines basic constants and POD structures for layout files,
// provides basic info about .nov and .exe layout files,
// and checks protected layout file passwords.

namespace nx::core::layout {

/* Max future nov file version that should be opened by the current client version. */
const qint32 kMaxVersion = 1024;

struct FileInfo;
struct CryptoInfo;

// Just checks extension.
NX_VMS_COMMON_API bool isLayoutExtension(const QString& fileName);

// Reads basic info for layout file. Allows .exe.tmp extension if allowTemp is set.
NX_VMS_COMMON_API std::optional<FileInfo> identifyFile(
    const QString& fileName, bool allowTemp = false);

// Checks a layout password against pre-loaded the hash.
NX_VMS_COMMON_API bool checkPassword(const QString& password, const CryptoInfo& cryptoInfo);


// TODO Increase this value to some suitable value, as we now use a new file after each codec change.
constexpr int kMaxStreams = 256;

// TODO: Sync with nov_launcher_win.cpp and nov_file_launcher project.
constexpr quint64 kFileMagic = 0x73a0b934820d4055ull;
constexpr int kHashSize = 32;

using Key = std::array<unsigned char, kHashSize>;

#pragma pack(push, 4)
struct StreamIndexEntry
{
    qint64 offset = 0;
    quint32 fileNameCrc = 0;
    quint32 reserved = 0;
};

// This is the outdated version of file header and index.
struct StreamIndex1
{
    static constexpr quint64 kIndexMagic1 = 0xfed8260da9eebc04ll;
    static constexpr quint64 kIndexCryptedMagic = 0xfed8260da9eebc03ll;
    quint64 magic = kIndexMagic1;
    quint32 version = 1;
    quint32 entryCount = 0;
    std::array<StreamIndexEntry, kMaxStreams> entries = {};
};

struct Header
{
    static constexpr quint64 kIndexMagic = 0xdea5489c3f44ca16ll;
    uint64_t magic = kIndexMagic;
    uint32_t version = 2; //< File structure version.
    uint64_t offset = 0; //< Offest to index data;
};

struct CryptoInfo
{
    Key passwordSalt = {};
    Key passwordHash = {};
    std::array<unsigned char, 256 - 2 * kHashSize> reserved = {};
};
#pragma pack(pop)

struct FileInfo
{
    int version = 2;
    std::optional<CryptoInfo> cryptoInfo;
    std::vector<StreamIndexEntry> entries;
};

} // namespace nx::core::layout
