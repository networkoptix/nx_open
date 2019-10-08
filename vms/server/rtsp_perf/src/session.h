#pragma once

#include <atomic>
#include <thread>
#include <chrono>

#include <QtCore/QString>

struct Session
{
public:
    struct Config
    {
        QString user;
        QString password;
        bool printTimestamps = false;
        std::chrono::milliseconds timeout{5000};
    };
public:
    void run(const QString& url, const Config& config, bool live);
    std::atomic<uint64_t> totalBytesRead = 0;
    std::atomic<uint64_t> failedCount = 0;
    std::atomic<bool> failed = true;
    std::thread worker;

private:
    bool processPacket(const uint8_t* data, int64_t size, const char* url);
    int64_t parsePacketTimestamp(const uint8_t* data, int64_t size);

private:
    Config m_config;
    bool m_newPacket = true;
    std::chrono::time_point<std::chrono::system_clock> m_lastFrameTime;
};
