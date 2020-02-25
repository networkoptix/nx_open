#include "report.h"

#include <iomanip>
#include <chrono>
#include <sstream>

std::string toString(std::chrono::system_clock::time_point time)
{
    using namespace std::chrono;
    const auto msec = duration_cast<milliseconds>(time.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(time);
    std::stringstream str;
    str << std::put_time(std::localtime(&t), "%F %T") << "." <<
        std::setfill('0') << std::setw(3) << msec.count();
    return str.str();
}

namespace detail {

void reportString(const QString& message)
{
    const auto begin = std::chrono::high_resolution_clock::now();

    printf("%s %s\n",
        toString(std::chrono::system_clock::now()).c_str(), message.toUtf8().constData());

    const auto end = std::chrono::high_resolution_clock::now();
    const auto reportDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    if (reportDuration > std::chrono::milliseconds(30))
        printf("WARNING: Printing to stdout is too slow: %lld ms.\n", reportDuration.count());
}

} // namespace detail
