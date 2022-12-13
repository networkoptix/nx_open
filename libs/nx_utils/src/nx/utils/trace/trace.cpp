// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "trace.h"

#include <fstream>
#include <array>
#include <cstdio>
#include <memory>
#include <mutex>

#include <concurrentqueue.h>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/thread_util.h>

using namespace std::chrono;

namespace nx::utils::trace {

namespace {

constexpr auto kCapacity = 10000;
constexpr auto kDumpPeriod = 1s;

class Queue
{
private:
    struct Event
    {
        Log::EventPhase phase;
        int64_t id; //< Thread id or object id.
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::time_point end;
        const char* name;
        const char* category;
        std::unique_ptr<QJsonObject> args;
    };

public:
    Queue(Queue const&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue const&) = delete;
    Queue& operator=(Queue &&) = delete;

    void startThread(const std::string& path)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_thread)
            return;

        m_thread = std::unique_ptr<QThread>(QThread::create(
            [this](std::string path)
            {
                while (!m_thread->isInterruptionRequested())
                {
                    std::this_thread::sleep_for(kDumpPeriod);
                    dump(path);
                }
            }, path));

        m_thread->setObjectName("TraceLog");

        m_thread->start();
    }

    void stopThread()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (!m_thread)
            return;

        m_thread->requestInterruption();
        m_thread->wait(duration_cast<milliseconds>(kDumpPeriod).count() * 2);
    }

    static Queue& instance()
    {
        static Queue i;
        return i;
    }

    void addCompleteEvent(
        const steady_clock::time_point& start,
        const steady_clock::time_point& end,
        const char* name,
        const char* category,
        std::unique_ptr<QJsonObject> args)
    {
        m_queue.try_enqueue({
            Log::EventPhase::Complete,
            static_cast<int64_t>(currentThreadSystemId()),
            start,
            end,
            name,
            category,
            std::move(args)
        });
    }

    void addEvent(
        Log::EventPhase phase,
        const steady_clock::time_point& timeStamp,
        const char* name,
        const char* category,
        int64_t id,
        std::unique_ptr<QJsonObject> args)
    {
        m_queue.try_enqueue({
            phase,
            id,
            timeStamp,
            timeStamp, //< 0 duration
            name,
            category,
            std::move(args)
        });
    }

private:
    Queue()
    {
    }

    ~Queue()
    {
        stopThread();
    }

    void dump(const std::string& path)
    {
        // This method is intended to be called from a single dumping thread.
        // Technically the queue supports multiple consumers, but it is not needed here.

        // Avoid placing the whole queue on the stack, use static storage.
        static std::array<Event, kCapacity> events;

        size_t count = m_queue.try_dequeue_bulk(&events[0], events.size());

        if (count == 0)
            return;

        NX_ASSERT(count < 0.9 * events.size(),
            "The trace event queue is almost full, consider increasing its size");

        std::ofstream ofs;
        ofs.open(path, std::ofstream::out | std::ofstream::app);

        if (!ofs)
            return;

        if (QFileInfo(path.c_str()).size() == 0)
            ofs << "[\n";

        const auto pid = QCoreApplication::applicationPid();

        for (size_t i = 0; i != count; ++i)
        {
            const auto& event = events[i];

            const auto ts = duration_cast<microseconds>(event.start.time_since_epoch()).count();

            ofs << "{\"pid\":" << pid
                << ",\"ts\":" << ts
                << ",\"ph\":\"" << static_cast<char>(event.phase) << "\"";
                    ofs << ",\"cat\":\"" << event.category << "\"";
            switch (event.phase)
            {
                case Log::EventPhase::Begin:
                    break;
                case Log::EventPhase::End:
                    break;
                case Log::EventPhase::Complete:
                {
                    ofs << ",\"tid\":" << event.id;
                    const auto dur = duration_cast<microseconds>(event.end - event.start).count();
                    ofs << ",\"dur\":" << dur;
                    break;
                }
                case Log::EventPhase::Instant:
                    ofs << ",\"tid\":" << event.id;
                    ofs << ",\"s\":\"p\"";
                    break;
                case Log::EventPhase::Counter:
                    if (event.id != -1)
                        ofs << ",\"id\":" << event.id;
                    break;
                case Log::EventPhase::BeginAsync:
                case Log::EventPhase::InstantAsync:
                case Log::EventPhase::EndAsync:
                    ofs << ",\"id2\":{\"local\":" << event.id << "}";
                    break;
                default:
                    break;
            }
            if (event.args)
            {
                const auto buf = QJsonDocument(*event.args.get()).toJson(QJsonDocument::Compact);
                ofs << ",\"args\": " << buf.constData();
            }

            ofs << ",\"name\":\"" << event.name << "\"},\n";
        }

        ofs.close();
    }

    moodycamel::ConcurrentQueue<Event> m_queue{kCapacity};

    std::mutex m_mutex;
    std::unique_ptr<QThread> m_thread;
};

} // namespace

std::atomic<bool> Log::m_enabled(false);

bool Log::enable(const std::string& path)
{
    // Ensure the queue is constructed before allowing producers to write.
    Queue::instance();

    bool expected = false;
    if (!m_enabled.compare_exchange_strong(expected, true))
        return false;

    if (!path.empty())
        std::remove(path.c_str());

    Queue::instance().startThread(path);
    return true;
}

void Log::disable()
{
    if (!isEnabled())
        return;

    Queue::instance().stopThread();
    m_enabled = false;
}

Log Log::event(EventPhase phase, const char* name)
{
    const auto id = static_cast<int64_t>(currentThreadSystemId());
    return Log(phase, name, id, steady_clock::now());
}

Log Log::eventAsync(EventPhase phase, const char* name, int64_t id)
{
    return Log(phase, name, id, steady_clock::now());
}

void Log::args(QJsonObject&& data)
{
    m_args = std::make_unique<QJsonObject>(std::move(data));
}

Log::Log(EventPhase phase, const char* name, int64_t id, std::chrono::steady_clock::time_point ts):
    m_phase(phase),
    m_name(name),
    m_id(id),
    m_timeStamp(ts)
{
}

Log::~Log()
{
    if (!Log::isEnabled())
        return;
    Queue::instance().addEvent(m_phase, m_timeStamp, m_name, "", m_id, std::move(m_args));
}

void Scope::args(QJsonObject&& data)
{
    *reinterpret_cast<ArgsType*>(&m_data) = std::make_unique<QJsonObject>(std::move(data));
}

void Scope::report()
{
    auto endTime = steady_clock::now();
    ArgsType* ptr = reinterpret_cast<ArgsType*>(&m_data);
    Queue::instance().addCompleteEvent(m_startTime, endTime, m_name, "", std::move(*ptr));
    ptr->~ArgsType();
}

} // namespace nx::utils::trace
