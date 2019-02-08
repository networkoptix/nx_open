#pragma once

namespace nx {
namespace sdk {

enum class Error
{
    noError,
    unknownError,
    needMoreBufferSpace,
    typeIsNotSupported,
    networkError,
};

static inline const char* toString(Error error)
{
    switch (error)
    {
        case Error::noError: return "noError";
        case Error::unknownError: return "unknownError";
        case Error::needMoreBufferSpace: return "needMoreBufferSpace";
        case Error::typeIsNotSupported: return "typeIsNotSupported";
        case Error::networkError: return "networkError";
        default: return "<unsupported Error>";
    }
}

} // namespace sdk
} // namespace nx
