#pragma once

#include <functional>
#include <memory>

struct AVIOContext;

namespace nx::media::ffmpeg {

class IoContext
{
public:
    IoContext(uint32_t bufferSize, bool writable);
    IoContext(const IoContext&) = delete;
    IoContext& operator=(const IoContext&) = delete;
    virtual ~IoContext();
    AVIOContext* getAvioContext();

    std::function<int(uint8_t* buffer, int size)> readHandler;
    std::function<int(uint8_t* buffer, int size)> writeHandler;
    std::function<int(int64_t pos, int whence)> seekHandler;

private:
    uint8_t* m_buffer = nullptr;
    AVIOContext* m_avioContext = nullptr;
};

using IoContextPtr = std::unique_ptr<IoContext>;

IoContextPtr openFile(const std::string& fileName);

} // namespace nx::media::ffmpeg
