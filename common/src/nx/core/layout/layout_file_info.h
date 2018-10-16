#pragma once
#include <array>

#include <QtCore/QtGlobal>

// This module defines basic constants and POD structures for layout files,
// provides basic info about .nov and .exe layout files,
// and checks protected layout file passwords.

namespace nx::core::layout {

struct FileInfo;

// Just checks extension.
bool isLayoutExtension(const QString& fileName);

// Reads basic info for layout file. Allows .exe.tmp extension if allowTemp is set.
FileInfo identifyFile(const QString& fileName, bool allowTemp = false);

// Checks a layout password against pre-loaded the hash.
bool checkPassword(const QString& password, const FileInfo& fileInfo);

constexpr int kMaxStreams = 256;

constexpr quint64 kFileMagic = 0x73a0b934820d4055ull;

constexpr quint64 kIndexMagic = 0xfed8260da9eebc04ll;
constexpr quint64 kIndexCryptedMagic = 0xfed8260da9eebc03ll;
constexpr int kHashSize = 32;

using Key = std::array<unsigned char, kHashSize>;

#pragma pack(push, 4)
struct StreamIndexEntry
{
    qint64 offset = 0;
    quint32 fileNameCrc = 0;
    quint32 reserved = 0;
};

struct StreamIndex
{
    quint64 magic = kIndexMagic;
    quint32 version = 1;
    quint32 entryCount = 0;
    std::array<StreamIndexEntry, kMaxStreams> entries = {};
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
    bool isValid = false;
    int version = 1;
    bool isCrypted = false;
    Key passwordSalt = {};
    Key passwordHash = {};
    qint64 offset = 0;
};

} // namespace nx::core::layout

