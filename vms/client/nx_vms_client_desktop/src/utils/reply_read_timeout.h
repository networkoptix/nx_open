#pragma once

#include <QTimer>
#include <QNetworkReply>

namespace nx::vms::client::desktop {
namespace utils {

/**
 * Aborts QNetworkReply if no readyRead signal was emitted within a certain timeout.
 */
class ReplyReadTimeout: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    QTimer m_timer;

public:
    ReplyReadTimeout(QNetworkReply* reply, const std::chrono::milliseconds timeout): QObject(reply)
    {
        if (!reply->isRunning())
            return;

        connect(reply, &QNetworkReply::finished, this, &QObject::deleteLater);
        connect(reply, &QNetworkReply::readyRead, &m_timer, qOverload<>(&QTimer::start));
        connect(&m_timer, &QTimer::timeout, this, [reply, this] {
            if (!reply->isRunning())
                return;

            reply->abort();
            m_timer.stop();
        });

        m_timer.start(timeout);
    }

    virtual ~ReplyReadTimeout() {}

    static void set(QNetworkReply* reply, const std::chrono::milliseconds timeout)
    {
        new ReplyReadTimeout(reply, timeout);
    }
};

} // namespace utils
} // namespace nx::vms::client::desktop
