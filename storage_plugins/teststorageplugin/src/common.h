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
