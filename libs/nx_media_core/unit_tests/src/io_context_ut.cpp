// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
} // extern "C"

#include <cerrno>
#include <new>
#include <stdexcept>
#include <vector>

#include <nx/media/ffmpeg/io_context.h>

namespace nx::media::ffmpeg::test {

static constexpr uint32_t kBufferSize = 1024;

TEST(FfmpegIoContext, writeHandlerExceptionBecomesAnFfmpegError)
{
    IoContext context(kBufferSize, /*writable*/ true, /*seekable*/ false);

    int calls = 0;
    context.writeHandler = [&calls](const uint8_t* /*buffer*/, int /*size*/) -> int
    {
        ++calls;
        throw std::bad_alloc();
    };

    AVIOContext* avio = context.getAvioContext();

    // More than the internal buffer holds, so ffmpeg has to reach the handler.
    const std::vector<uint8_t> data(kBufferSize * 2, 0x42);
    avio_write(avio, data.data(), (int) data.size());
    avio_flush(avio);

    ASSERT_GT(calls, 0);
    ASSERT_EQ(AVERROR(ENOMEM), avio->error);
}

TEST(FfmpegIoContext, readHandlerExceptionBecomesAnFfmpegError)
{
    IoContext context(kBufferSize, /*writable*/ false, /*seekable*/ false);

    context.readHandler = [](uint8_t* /*buffer*/, int /*size*/) -> int
    {
        throw std::runtime_error("the read handler has failed");
    };

    uint8_t buffer[16];
    ASSERT_EQ(AVERROR(EIO), avio_read(context.getAvioContext(), buffer, (int) sizeof(buffer)));
}

TEST(FfmpegIoContext, anExceptionDoesNotEscapeTheDestructor)
{
    // ~IoContext() calls avio_flush(), which reaches the write handler with the data still
    // buffered. An exception escaping a destructor calls std::terminate(), so this test fails
    // by aborting the process rather than by a failed assertion.
    IoContext context(kBufferSize, /*writable*/ true, /*seekable*/ false);

    context.writeHandler = [](const uint8_t* /*buffer*/, int /*size*/) -> int
    {
        throw std::bad_alloc();
    };

    const uint8_t byte = 0x42;
    avio_write(context.getAvioContext(), &byte, 1);
}

TEST(FfmpegIoContext, aHandlerWhichDoesNotThrowIsUnaffected)
{
    IoContext context(kBufferSize, /*writable*/ true, /*seekable*/ false);

    std::vector<uint8_t> written;
    context.writeHandler = [&written](const uint8_t* buffer, int size) -> int
    {
        written.insert(written.end(), buffer, buffer + size);
        return size;
    };

    AVIOContext* avio = context.getAvioContext();

    const std::vector<uint8_t> data(kBufferSize * 2, 0x42);
    avio_write(avio, data.data(), (int) data.size());
    avio_flush(avio);

    ASSERT_EQ(0, avio->error);
    ASSERT_EQ(data, written);
}

} // namespace nx::media::ffmpeg::test
