// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Provide a no-op fallback so non-Qt builds (e.g., nov_file_launcher) can include this header.
#ifndef NX_VMS_COMMON_API
    #define NX_VMS_COMMON_API
#endif

// This module defines basic constants and POD structures for layout files,
// provides basic info about .nov and .exe layout files,
// and checks protected layout file passwords.

namespace nx::core::layout {

struct FileInfo;
struct CryptoInfo;

// Just checks extension.
NX_VMS_COMMON_API bool isLayoutExtension(const std::string& fileName);

// Reads basic info for layout file. Allows .exe.tmp extension if allowTemp is set.
NX_VMS_COMMON_API std::optional<FileInfo> readNovFileIndex(
    const std::string& fileName, bool allowTemp = false);

// Append basic info for layout file to end of file.
NX_VMS_COMMON_API bool writeNovFileIndex(const std::string& fileName, const FileInfo& info);

// Checks a layout password against pre-loaded the hash.
NX_VMS_COMMON_API bool checkPassword(const std::string& password, const CryptoInfo& cryptoInfo);

static constexpr int kHashSize = 32;
static constexpr int kCryptoInfoSize = 256;

using Key = std::array<unsigned char, kHashSize>;

#pragma pack(push, 4)

struct StreamIndexEntry
{
    int64_t offset = 0;
    uint32_t fileNameCrc = 0;
    uint32_t reserved = 0;
};

struct CryptoInfo
{
    Key passwordSalt = {};
    Key passwordHash = {};
    std::array<unsigned char, kCryptoInfoSize - 2 * kHashSize> reserved = {};
};

struct Header
{
    uint32_t hasCryptoData = 0; //< 0 - no crypto data, 1 - crypto data present before index.
    uint32_t entryCount = 0; //< Number of index elements.
};

struct Footer
{
    static constexpr uint64_t kIndexMagic = 0xdea5489c3f44ca16ull;
    static constexpr uint64_t kCurrentVersion = 2;
    uint64_t magic = kIndexMagic;
    uint32_t version = 2;
    uint64_t offset = 0; //< Offset to Header data.
};

#pragma pack(pop)

/*
 * Current NOV file storage structure:
 * | exe(optional) | filename | data | filename | data | ... | Header | CryptoInfo(optional) | StreamIndexEntry | ... | Footer |
 */
struct NX_VMS_COMMON_API FileInfo
{
    int totalIndexHeaderSize();
    std::optional<CryptoInfo> cryptoInfo;
    std::vector<StreamIndexEntry> entries;
};

} // namespace nx::core::layout
