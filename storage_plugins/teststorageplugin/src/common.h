#pragma once

#include <atomic>
#include <cstdio>

#define ERROR_CODE_LIST(APPLY) \
    APPLY(ok)

#define ERROR_CODE_APPLY_ENUM(value) value,

enum ErrorCode
{
    ERROR_CODE_LIST(ERROR_CODE_APPLY_ENUM)
};

const char* errorCodeToString(ErrorCode ecode);

#define _TEST_STORAGE_DEBUG

#if defined (_TEST_STORAGE_DEBUG)
#   define LOG(fmtString, ...) do { \
        char buf[1024]; \
        sprintf(buf, "%s ", __FUNCTION__); \
        sprintf(buf + strlen(buf), fmtString, __VA_ARGS__); \
        fprintf(stderr, "%s\n", buf); \
        fflush(stderr); \
    } while (0)
#else
#   define LOG(...)
#endif

template <typename P>
class PluginRefCounter
{
public:
    PluginRefCounter()
        : m_count(1)
    {}

    int pAddRef() { return ++m_count; }

    int pReleaseRef()
    {
        int new_count = --m_count;
        if (new_count <= 0) 
            delete static_cast<P*>(this);

        return new_count;
    }
private:
    std::atomic<int> m_count;
}; // class PluginRefCounter
