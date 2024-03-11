// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// #define UDT_LOG

#ifdef UDT_LOG

#include <format>
#include <list>
#include <mutex>
#include <ostream>

namespace UDT {

class Log
{
public:
    class Stream
    {
    public:
        virtual void write(const std::string& str) = 0;
    };

    static void addStream(std::shared_ptr<Stream> stream);

    static void write(const std::string& str);

private:
    static Log& instance();

private:
    std::mutex m_streamsMutex;
    std::list<std::shared_ptr<Stream>> m_streams;
};

} // namespace UDT

#endif // UDT_LOG

#ifdef UDT_LOG
#define UDT_LOG_WRITE(LEVEL, STD_FORMAT_STRING, ...) \
do { \
    static const char * fmt = "udt " LEVEL ": " STD_FORMAT_STRING "\n"; \
    UDT::Log::write( \
        std::vformat( \
            fmt, \
            std::make_format_args(__VA_ARGS__)) \
    ); \
} while(0)
#else
#define UDT_LOG_WRITE(...)
#endif // def UDT_LOG

//-------------------------------------------------------------------------------------------------

#define UDT_LOG_WARN(STD_FORMAT_STRING, ...) UDT_LOG_WRITE("WARN", STD_FORMAT_STRING, __VA_ARGS__)

#define UDT_LOG_DBG(STD_FORMAT_STRING, ...) UDT_LOG_WRITE("DBG ", STD_FORMAT_STRING, __VA_ARGS__)
