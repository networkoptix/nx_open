#include "malloc_info.h"

#ifdef __linux__
#include <malloc.h>
#include <stdio.h>
#endif

namespace nx::utils::memory {

bool mallocInfo(
    std::string* data,
    std::string* contentType)
{
#ifdef __linux__
    char* buf = nullptr;
    std::size_t bufSize = 0;
    FILE* outputStream = open_memstream(&buf, &bufSize);
    if (outputStream == nullptr)
        return false;

    if (malloc_info(0 /*options*/, outputStream) != 0)
    {
        fclose(outputStream);
        free(buf);
        return false;
    }

    fflush(outputStream);
    fclose(outputStream);

    if (buf == nullptr)
        return false;

    *contentType = "text/xml";
    *data = buf;

    free(buf);

    return true;
#else
    *contentType = "text/plain";
    *data = "not implemented";

    return true;
#endif
}

} // namespace nx::utils::memory
