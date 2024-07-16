// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <string>

struct AVIOContext;

namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API IoContext
{
public:
    IoContext(uint32_t bufferSize, bool writable, bool seekable);
    IoContext(const IoContext&) = delete;
    IoContext& operator=(const IoContext&) = delete;
    virtual ~IoContext();
    AVIOContext* getAvioContext();

    std::function<int(uint8_t* buffer, int size)> readHandler;
    std::function<int(const uint8_t* buffer, int size)> writeHandler;
    std::function<int(int64_t pos, int whence)> seekHandler;

private:
    uint8_t* m_buffer = nullptr;
    AVIOContext* m_avioContext = nullptr;
};

using IoContextPtr = std::unique_ptr<IoContext>;

IoContextPtr NX_MEDIA_CORE_API openFile(const std::string& fileName);
IoContextPtr NX_MEDIA_CORE_API fromBuffer(const uint8_t* data, int size);
IoContextPtr NX_MEDIA_CORE_API createFile(const std::string& fileName);

} // namespace nx::media::ffmpeg
