#pragma once

#include <chrono>

#include <QtCore/QElapsedTimer>

namespace nx {
namespace utils {

class NX_UTILS_API ElapsedTimer
{
public:
    ElapsedTimer();

    bool hasExpired(std::chrono::milliseconds value) const;
    std::chrono::milliseconds restart();
    void invalidate();
    bool isValid() const;
    std::chrono::milliseconds elapsed() const;
    qint64 elapsedMs() const;
private:
    QElapsedTimer m_timer;
};

} // namespace utils
} // namespace nx
