#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

#include <QtCore/QString>
#include <QtCore/QStringList>

class RtspPerf
{
public:
    struct Config
    {
        QString server = "127.0.0.1:7001";
        QString user = "admin";
        QString password;
        QStringList urls;
        int count = 1;
        int livePercent = 100;
        bool useSsl = false;
        std::chrono::milliseconds timeout{5000};
        std::chrono::milliseconds startInterval{0};
        bool printTimestamps = false;
        bool disableRestart = false;
    };

public:
    RtspPerf(const Config& config);
    void run();

private:
    bool getCamerasUrls(const QString& server, std::vector<QString>& cameras);
    void startSessionsThread(const std::vector<QString>& urls);

    struct Session
    {
        void run(const QString& url, const Config& config, bool live);
        std::atomic<uint64_t> totalBytesRead = 0;
        std::atomic<uint64_t> failedCount = 0;
        std::atomic<bool> failed = true;
        std::thread worker;
    private:
        void parsePacketTimestamp(
            const uint8_t* data,
            int64_t size,
            const QString& cameraId,
            std::chrono::milliseconds timeout);
        bool newPacket = true;
        std::chrono::time_point<std::chrono::system_clock> m_lastFrameTime;
    };

private:
    std::vector<Session> m_sessions;
    std::chrono::milliseconds m_currentStartInterval;
    Config m_config;
};
