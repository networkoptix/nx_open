// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_context.h"

#include <fstream>

extern "C" {

#include <libavutil/opt.h>
#include <libavformat/avio.h>

} // extern "C"

namespace nx::media::ffmpeg {

static int ffmpegRead(void *opaque, uint8_t* buffer, int size);
static int ffmpegWrite(void *opaque, uint8_t* buf, int size);
static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence);

IoContext::IoContext(uint32_t bufferSize, bool writable, bool seekable)
{
    m_buffer = (uint8_t*) av_malloc(bufferSize);
    m_avioContext = avio_alloc_context(
        m_buffer,
        bufferSize,
        writable ? 1 : 0,
        this,
        &ffmpegRead,
        &ffmpegWrite,
        seekable ? &ffmpegSeek : nullptr);
}

IoContext::~IoContext()
{
    avio_flush(m_avioContext);
    m_avioContext->opaque = 0;
    av_freep(&m_avioContext->buffer);
    av_opt_free(m_avioContext);
    av_free(m_avioContext);
}

AVIOContext* IoContext::getAvioContext()
{
    return m_avioContext;
}

static int ffmpegRead(void *opaque, uint8_t* buffer, int size)
{
    IoContext* owner = reinterpret_cast<IoContext*>(opaque);
    if (!owner->readHandler)
        return -1;
    auto bytesRead = owner->readHandler(buffer, size);
    return bytesRead == 0 ? AVERROR_EOF : bytesRead;
}

static int ffmpegWrite(void *opaque, uint8_t* buffer, int size)
{
    IoContext* owner = reinterpret_cast<IoContext*>(opaque);
    if (!owner->writeHandler)
        return -1;
    return owner->writeHandler(buffer, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    IoContext* owner = reinterpret_cast<IoContext*>(opaque);
    if (!owner->seekHandler)
        return -1;
    return owner->seekHandler(pos, whence);
}

IoContextPtr openFile(const std::string& fileName)
{
    auto file = std::make_shared<std::ifstream>(fileName, std::ios::binary);
    if (!file->is_open())
        return nullptr;

    auto ioContext = std::make_unique<IoContext>(1024*64, /*writable*/false, /*seekable*/true);
    ioContext->readHandler =
        [file](uint8_t* buffer, int size)
        {
            file->read((char*)buffer, size);
            return (int)file->gcount();
        };

    ioContext->seekHandler =
        [file] (int64_t pos, int whence)
        {
            switch (whence)
            {
                if (file->eof())
                    file->clear();
                case SEEK_SET:
                    file->seekg(pos, std::ios_base::beg);
                    break;
                case SEEK_CUR:
                    file->seekg(pos, std::ios_base::cur);
                    break;
                case SEEK_END:
                    file->seekg(pos, std::ios_base::end);
                    break;
                default:
                    return AVERROR(ENOENT);
            }
            return (int)file->tellg();
        };
    return ioContext;
}

IoContextPtr fromBuffer(const uint8_t* data, int size)
{
    struct MemoryFile
    {
        const uint8_t* data = nullptr;
        int size = 0;
        int pos = 0;
    };
    auto memoryFile = std::make_shared<MemoryFile>();
    memoryFile->size = size;
    memoryFile->data = data;

    auto ioContext = std::make_unique<IoContext>(1024*64, /*writable*/false, /*seekable*/true);
    ioContext->readHandler =
        [memoryFile](uint8_t* buffer, int size)
        {
            auto actualSize = std::min(memoryFile->size - memoryFile->pos, size);
            memcpy(buffer, memoryFile->data + memoryFile->pos, actualSize);
            memoryFile->pos += actualSize;
            return actualSize;
        };

    ioContext->seekHandler =
        [memoryFile] (int64_t pos, int whence)
        {
            switch (whence)
            {
                case SEEK_SET:
                    memoryFile->pos = pos;
                    break;
                case SEEK_CUR:
                    memoryFile->pos += pos;
                    break;
                case SEEK_END:
                    memoryFile->pos = memoryFile->size - pos;
                    break;
                default:
                    return AVERROR(ENOENT);
            }
            return memoryFile->pos;
        };
    return ioContext;

}

IoContextPtr createFile(const std::string& fileName)
{
    auto file = std::make_shared<std::ofstream>(fileName, std::ios::binary);
    if (!file->is_open())
        return nullptr;

    auto ioContext = std::make_unique<IoContext>(1024*64, /*writable*/true, /*seekable*/true);
    ioContext->writeHandler =
        [file](uint8_t* buffer, int size)
        {
            file->write((char*)buffer, size);
            return size;
        };

    ioContext->seekHandler =
        [file] (int64_t pos, int whence)
        {
            switch (whence)
            {
                case SEEK_SET:
                    file->seekp(pos, std::ios_base::beg);
                    break;
                case SEEK_CUR:
                    file->seekp(pos, std::ios_base::cur);
                    break;
                case SEEK_END:
                    file->seekp(pos, std::ios_base::end);
                    break;
                default:
                    return AVERROR(ENOENT);
            }
            return (int)file->tellp();
        };
    return ioContext;
}

} // namespace nx::media::ffmpeg
