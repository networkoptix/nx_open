#pragma once

#include <QtCore/QElapsedTimer>
#include <chrono>

namespace nx {
namespace utils {

class NX_UTILS_API ElapsedTimer
{
public:
    ElapsedTimer();

    bool hasExpired(std::chrono::milliseconds value) const;
    void restart();
private:
    QElapsedTimer m_timer;
};

} // namespace utils
} // namespace nx
