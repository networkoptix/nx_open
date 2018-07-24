#include "error.h"

extern "C"{
#include <libavutil/error.h>
}

namespace nx{
namespace ffmpeg {
namespace error {

namespace {
int err = 0;
}

std::string toString(int errorCode)
{
    static constexpr int length = AV_ERROR_MAX_STRING_SIZE;
    char errorBuffer[length];
    av_strerror(errorCode, errorBuffer, length);
    return std::string(errorBuffer);
}

bool updateIfError(int errorCode) 
{
    bool error = errorCode < 0;
    if(error)
        setLastError(errorCode);
    return error;
}

void setLastError(int errorCode)
{
    err = errorCode;
}

int lastError()
{
    return err;
}

bool hasError()
{
    return err < 0;
}

} // namespace error
} // namespace ffmpeg
} // namespace nx