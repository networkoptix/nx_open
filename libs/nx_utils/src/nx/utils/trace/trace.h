// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <stdint.h>
#include <memory>
#include <string>
#include <type_traits>

class QJsonObject;

namespace nx::utils::trace {

// Time trace functions and macros, use chrome://tracing for visualization.

// Performance goals
//
// * Avoid thread blocking when logging trace events - some events have high frequency.
// * Avoid dynamic memory allocations unless it is explicitly required.
// * When tracing is disabled the overhead should be as little as checking a single variable.
// * Minimize compilation time - the header is intended to be frequently included.
//
// Basic design
//
// Each call to the tracing API posts an event record into multi-producer, multi-consumer
// lock-free concurrent queue. There is a single thread which clears the queue (by reading all
// the events) and dumps the new events into a file. The dump happens with a fixed delay.
// So there is a maximum number of events per seconds which can be generated by the application.
// This is OK since Chrome can not consume tracing files which are large enough (depends on RAM).
// The assert is generated by the dumping thread whenever the queue is more than 90% full.
//
// Scope::Scope(), Scope::~Scope() and Log::isEnabled() should be inlined because we would like
// to do as little work as possible when profiling is not enabled.

// Main class for logging trace events. Direct usage is only intended for enabling/disabling
// trace recording for the application. For logging trace events use TRACE_* macros below.
class NX_UTILS_API Log
{
public:

    // Constants from the Trace Event Format.
    // https://github.com/catapult-project/catapult/wiki/Trace-Event-Format
    enum class EventPhase
    {
        Begin = 'B',
        End = 'E',
        Complete = 'X',
        Instant = 'I',
        Counter = 'C',
        BeginAsync = 'b',
        InstantAsync = 'n',
        EndAsync = 'e',
    };

    Log() = delete;
    Log(Log const&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log const&) = delete;
    Log& operator=(Log&&) = delete;

    static bool enable(const std::string& path);
    static void disable();

    static bool isEnabled()
    {
        return m_enabled.load();
    }

    static Log eventAsync(EventPhase phase, const char* name, int64_t id);

    static Log event(EventPhase phase, const char* name);

    void args(QJsonObject&& data);

    ~Log();

private:
    Log(EventPhase phase, const char* name, int64_t id, std::chrono::steady_clock::time_point ts);

    static std::atomic<bool> m_enabled;

    EventPhase m_phase;
    const char* m_name;
    int64_t m_id;
    std::chrono::steady_clock::time_point m_timeStamp;
    std::unique_ptr<QJsonObject> m_args;
};

// Helper class for convenient tracking of execution time of some scope (eg. function).
class NX_UTILS_API Scope
{
    using ArgsType = std::unique_ptr<QJsonObject>;

public:
    Scope(Scope const&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope const&) = delete;
    Scope& operator=(Scope&&) = delete;

    Scope(const char* name)
    {
        if (!Log::isEnabled())
        {
            m_name = nullptr;
            return;
        }

        m_name = name;
        m_startTime = std::chrono::steady_clock::now();

        // It is not possible to inline destructor code with incomplete QJsonObject.
        // However, including full QJsonObject header significantly decreases
        // compilation time.
        // Avoid this by moving the destructor call to the actual trace report call
        // which is not inlined.
        // Additional benefit - save unique_ptr construction/deconstruction calls
        // when tracing is disabled.
        new(static_cast<void*>(&m_data)) ArgsType();
    }

    ~Scope()
    {
        if (!m_name)
            return;
        report();
    }

    void args(QJsonObject&& data);

private:
    void report();

    std::chrono::steady_clock::time_point m_startTime;
    const char* m_name;
    std::aligned_storage_t<sizeof(ArgsType), alignof(ArgsType)> m_data;
};

} // namespace nx::utils::trace

// Generate unique name for scope variable using current line number.
#define _NX_TRACE_CONCAT_(x, y) x##y
#define _NX_TRACE_CONCAT(x, y) _NX_TRACE_CONCAT_(x, y)
#define _NX_TRACE_UNIQUE(varname) _NX_TRACE_CONCAT(varname, __LINE__)

// Check that legal constexpr string is passed as event name.
#define _NX_TRACE_CHECK_LITERAL(name) \
    if constexpr([&]{ \
        static_assert( \
            std::string_view(name).find_first_of("\\\"") == std::string_view::npos, \
            R"(name must be a constexpr string without " and \)"); \
        return true; \
    }())

// Convenience macros for generating trace events.

// Complete event
// Usage:
//   NX_TRACE_SCOPE("event").args({{"location": "path"}});
//   where .args() call is optional.
#define NX_TRACE_SCOPE(name) \
    nx::utils::trace::Scope _NX_TRACE_UNIQUE(_trace_scope)(name); \
    _NX_TRACE_CHECK_LITERAL(name) \
    if (nx::utils::trace::Log::isEnabled()) _NX_TRACE_UNIQUE(_trace_scope)

#define _NX_TRACE_EVENT(phase, name) \
    _NX_TRACE_CHECK_LITERAL(name) \
    if (nx::utils::trace::Log::isEnabled()) \
        nx::utils::trace::Log::event(nx::utils::trace::Log::Event##phase, (name))

#define _NX_TRACE_EVENT_ASYNC(phase, name, id) \
    _NX_TRACE_CHECK_LITERAL(name) \
    if (nx::utils::trace::Log::isEnabled()) \
        nx::utils::trace::Log::eventAsync(nx::utils::trace::Log::Event##phase, (name), (id))

// Other events
#define NX_TRACE_COUNTER(name) _NX_TRACE_EVENT_ASYNC(Phase::Counter, (name), -1)
#define NX_TRACE_COUNTER_ID(name, id) _NX_TRACE_EVENT_ASYNC(Phase::Counter, (name), (id))
#define NX_TRACE_BEGIN(name) _NX_TRACE_EVENT(Phase::Begin, (name))
#define NX_TRACE_END(name) _NX_TRACE_EVENT(Phase::End, (name))
#define NX_TRACE_INSTANT(name) _NX_TRACE_EVENT(Phase::Instant, (name))
#define NX_TRACE_BEGIN_ASYNC(name, id) _NX_TRACE_EVENT_ASYNC(Phase::BeginAsync, (name), (id))
#define NX_TRACE_END_ASYNC(name, id) _NX_TRACE_EVENT_ASYNC(Phase::EndAsync, (name), (id))
#define NX_TRACE_INSTANT_ASYNC(name, id) _NX_TRACE_EVENT_ASYNC(Phase::InstantAsync, (name), (id))
