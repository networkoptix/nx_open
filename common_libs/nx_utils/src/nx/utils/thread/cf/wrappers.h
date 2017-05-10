#pragma once

#include "cfuture.h"
#include <nx/utils/system_error.h>

namespace cf {

template<typename Sender, typename Buffer>
cf::future<std::pair<SystemError::ErrorCode, size_t>> sendAsync(Sender* sender, const Buffer& buffer)
{
    auto p = std::make_shared<cf::promise<std::pair<SystemError::ErrorCode, size_t>>>();
    auto result = p->get_future();
    sender->sendAsync(
        buffer,
        [p](SystemError::ErrorCode ecode, size_t bytesSent) mutable
        {
            p->set_value(std::make_pair(ecode, bytesSent));
        });
    return result;
}

template<typename Reader, typename Buffer>
cf::future<std::pair<SystemError::ErrorCode, size_t>> readAsync(Reader* reader, Buffer* const buffer)
{
    auto p = std::make_shared<cf::promise<std::pair<SystemError::ErrorCode, size_t>>>();
    auto result = p->get_future();
    reader->readSomeAsync(
        buffer,
        [p](SystemError::ErrorCode ecode, size_t bytesRead) mutable
        {
            p->set_value(std::make_pair(ecode, bytesRead));
        });
    return result;
}


template<typename F>
cf::future<cf::unit> doWhile(F f)
{
    return f().then(
        [f](cf::future<bool> notDone)
        {
            return notDone.get() ? doWhile(f) : cf::make_ready_future<>(cf::unit());
        });
}

}