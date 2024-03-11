// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log.h"

#include <iostream>

#ifdef UDT_LOG

namespace UDT {

static void initializeOnce(Log* log)
{
    static std::atomic_bool klogInitialized = false;
    if (klogInitialized)
        return;
    klogInitialized = true;

    class Cerr: public Log::Stream
    {
    public:
        virtual void write(const std::string& str) override
        {
            std::cerr << str;
        }
    };

    log->addStream(std::make_unique<Cerr>());
}


Log& Log::instance()
{
    static Log log;
    initializeOnce(&log);
    return log;
}

void Log::addStream(std::shared_ptr<Log::Stream> stream)
{
    auto& log = instance();

    std::lock_guard<std::mutex> lock(log.m_streamsMutex);

    log.m_streams.push_back(stream);
}

void Log::write(const std::string& str)
{
    auto& log = instance();

    log.m_streamsMutex.lock();
    auto streams = log.m_streams;
    log.m_streamsMutex.unlock();

    for(auto& stream: streams)
        stream->write(str);
}

} // namespace UDT

#endif // #ifdef UDT_LOG
