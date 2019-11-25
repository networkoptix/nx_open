#include <functional>

struct AVIOContext;

namespace nx::vms::server::http_audio {

class FfmpegIoContext
{
public:
    FfmpegIoContext(uint32_t bufferSize, bool writable);
    FfmpegIoContext(const FfmpegIoContext&) = delete;
    FfmpegIoContext& operator=(const FfmpegIoContext&) = delete;
    virtual ~FfmpegIoContext();
    AVIOContext* getAvioContext();

    std::function<int(uint8_t* buffer, int size)> readHandler;
    std::function<int(uint8_t* buffer, int size)> writeHandler;
    std::function<int(int64_t pos, int whence)> seekHandler;

private:
    uint8_t* m_buffer = nullptr;
    AVIOContext* m_avioContext = nullptr;
};

using FfmpegIoContextPtr = std::unique_ptr<FfmpegIoContext>;

} // namespace nx::vms::server::http_audio
