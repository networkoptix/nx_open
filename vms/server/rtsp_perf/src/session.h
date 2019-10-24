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
        std::chrono::microseconds maxTimestampDiff{0};
        std::chrono::microseconds minTimestampDiff{0};
    };
public:
    void run(const QString& url, const Config& config, bool live);
    std::atomic<uint64_t> totalBytesRead = 0;
    std::atomic<uint64_t> failedCount = 0;
    std::atomic<bool> failed = true;
    std::thread worker;

private:
    struct Packet
    {
        int64_t timestampUs = -1;
        uint16_t flags = 0;
    };
private:
    bool processPacket(const uint8_t* data, int64_t size, const char* url);
    bool parsePacket(const uint8_t* data, int64_t size, Packet& packet);
    void checkDiff(std::chrono::microseconds diff, int64_t timestampUs, const char* url);

private:
    int64_t m_prevTimestampUs = -1;
    Config m_config;
    bool m_newPacket = true;
    std::chrono::time_point<std::chrono::system_clock> m_lastFrameTime;
};
