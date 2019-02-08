#pragma once

#include <string>
#include <vector>
#include <atomic>

#include <QtCore/QString>

class RtspPerf
{
public:
    struct Config
    {
        QString server = "rtsp://127.0.0.1:7001/";
        QString user = "admin";
        QString password = "qweasd123";
        int count = 1;
        int livePercent = 100;
        bool useSsl = false;

        QString toString()
        {
            return QString("Config:") +
                "\nserver: " + server +
                "\nuser: " + user +
                "\npassword: " + password +
                "\nuseSsl: " + useSsl +
                "\ncount: " + QString::number(count) +
                "\nlivePercent: " + QString::number(livePercent) +
                "\n";
        }
    };
public:
    RtspPerf(const Config& config): m_config(config) {}
    void run();

private:
    bool getCamerasUrls(const QString& server, std::vector<QString>& cameras);
    void startSession(const QString& url, bool live);

    struct Statistic
    {
        std::atomic<uint64_t> totalBytesRead = 0;
        std::atomic<uint64_t> failedStreams = 0;
        std::atomic<uint64_t> successStreams = 0;
    };

private:
    Config m_config;
    Statistic m_stat;
};
