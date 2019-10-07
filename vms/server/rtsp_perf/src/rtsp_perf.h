#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

#include <QtCore/QString>
#include <QtCore/QStringList>

#include "session.h"

class RtspPerf
{
public:
    struct Config
    {
        QString server;
        QStringList urls;
        int count = 1;
        int livePercent = 100;
        bool useSsl = false;
        std::chrono::milliseconds startInterval{0};
        bool disableRestart = false;
        Session::Config sessionConfig;
    };

public:
    RtspPerf(const Config& config);
    void run();

private:
    bool getCamerasUrls(const QString& server, std::vector<QString>& cameras);
    void startSessionsThread(const std::vector<QString>& urls);

private:
    std::vector<Session> m_sessions;
    std::chrono::milliseconds m_currentStartInterval;
    Config m_config;
};
