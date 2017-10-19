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
    void restart();
    void invalidate();

private:
    QElapsedTimer m_timer;
};

} // namespace utils
} // namespace nx
