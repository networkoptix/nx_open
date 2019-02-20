#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>

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
            return
                "\nserver: " + server +
                "\nuser: " + user +
                "\npassword: " + password +
                "\nuse ssl: " + QString::number(useSsl) +
                "\ncount: " + QString::number(count) +
                "\nlive percent: " + QString::number(livePercent) + "%"
                "\n";
        }
    };
public:
    RtspPerf(const Config& config): m_sessions(config.count), m_config(config) {}
    void run();

private:
    bool getCamerasUrls(const QString& server, std::vector<QString>& cameras);
    void startSessions(const std::vector<QString>& urls);

    struct Session
    {
        void run(const QString& url, const QString& user, const QString& password, bool live);
        std::atomic<uint64_t> totalBytesRead = 0;
        std::atomic<bool> failed = true;
        std::thread worker;
    };

private:
    std::vector<Session> m_sessions;
    std::atomic<int64_t> m_totalFailed = 0;
    Config m_config;
};
