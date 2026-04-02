// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFile>

#include "layout_file_info.h"

namespace nx::core::layout {

std::optional<FileInfo> parseVerson1(QFile&& file, const bool isExe);

#pragma pack(push, 4)

// This is the outdated version of file header and index.
struct StreamIndexV1
{
    static constexpr quint64 kFileMagic = 0x73a0b934820d4055ull;
    static constexpr quint64 kIndexMagic1 = 0xfed8260da9eebc04ll;
    static constexpr quint64 kIndexCryptedMagic = 0xfed8260da9eebc03ll;
    static constexpr int kMaxStreams = 256;
    quint64 magic = kIndexMagic1;
    quint32 version = 1;
    quint32 entryCount = 0;
    std::array<StreamIndexEntry, kMaxStreams> entries = {};
};

#pragma pack(pop)

} // namespace nx::core::layout
