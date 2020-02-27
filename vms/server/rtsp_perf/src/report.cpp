#include "report.h"

#include <iomanip>
#include <chrono>
#include <sstream>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>

#include "rtsp_perf_ini.h"

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

static std::string nowStr()
{
    return toString(std::chrono::system_clock::now());
}

class Reporter: public QnLongRunnable
{
public:
    Reporter() {}

    virtual ~Reporter() override
    {
        NX_WARNING(NX_SCOPE_TAG, "While printing the report to stdout: "
            "max print duration was %1 ms, max message queue size was %2.",
            m_maxPrintDuration.count(), m_maxMessageQueueSize);

        m_messageQueue.setTerminated(true); //< Unblock the current `pop()` call, if any.
        stop();
    }

    void addMessageToQueue(const QString& message)
    {
        m_messageQueue.push(message);
        m_maxMessageQueueSize = std::max(m_maxMessageQueueSize, m_messageQueue.size());
    }

    void printMessage(const QString& message)
    {
        const auto begin = std::chrono::steady_clock::now();

        printf("%s %s\n", nowStr().c_str(), message.toUtf8().constData());

        const auto end = std::chrono::steady_clock::now();
        const auto printDuration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

        if (printDuration > std::chrono::milliseconds(ini().printDurationToWarnMs))
        {
            NX_WARNING(NX_SCOPE_TAG,
                "Printing the report line to stdout took %1 ms which is more than %2 ms.",
                printDuration.count(), ini().printDurationToWarnMs);
        }

        m_maxPrintDuration = std::max(m_maxPrintDuration, printDuration);
    }

protected:
    virtual void run() override
    {
        while (!needToStop())
        {
            QString message;
            if (!m_messageQueue.pop(message))
                continue;

            printMessage(message);
        }
    }

private:
    QnSafeQueue<QString> m_messageQueue;
    int m_maxMessageQueueSize = 0;
    std::chrono::milliseconds m_maxPrintDuration = std::chrono::milliseconds(0);
};

void reportString(const QString& message)
{
    // Reporter will be created on first need.
    static std::unique_ptr<Reporter> reporter;
    if (!reporter)
    {
        reporter = std::make_unique<Reporter>();
        if (ini().useAsyncReporting)
        {
            NX_WARNING(NX_SCOPE_TAG, "Printing the report to stdout in a dedicated thread.");
            reporter->start(); //< Start the printing thread.
        }
        else
        {
            NX_WARNING(NX_SCOPE_TAG, "Printing the report to stdout synchronously.");
        }
    }

    if (ini().useAsyncReporting)
        reporter->addMessageToQueue(message);
    else
        reporter->printMessage(message);
}

} // namespace detail
