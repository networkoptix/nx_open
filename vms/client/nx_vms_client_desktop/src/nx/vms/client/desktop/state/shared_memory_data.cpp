// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_memory_data.h"

#include <range/v3/algorithm/find_if.hpp>

#include <nx/reflect/json.h>
#include <nx/utils/log/assert.h>

void serialize(nx::reflect::json::SerializationContext* ctx,
    const nx::vms::client::desktop::SharedMemoryData::ScreenUsageData& data)
{
    ctx->composer.startArray();
    for (const auto& item: data)
        ctx->composer.writeInt(item);
    ctx->composer.endArray();
}

namespace nx::vms::client::desktop {

const SharedMemoryData::Process* SharedMemoryData::findProcess(PidType pid) const
{
    auto process = ranges::find_if(processes,
        [=](const Process& process) { return process.pid == pid; });

    return process != processes.end() ? &(*process) : nullptr;
}

SharedMemoryData::Process* SharedMemoryData::findProcess(PidType pid)
{
    // It is safe to const-cast because current method is non-const.
    return const_cast<Process*>(static_cast<const SharedMemoryData*>(this)->findProcess(pid));
}

SharedMemoryData::Process* SharedMemoryData::addProcess(PidType pid)
{
    NX_ASSERT(!findProcess(pid), "Process %1 already exists");

    auto empty = findProcess(PidType());
    if (!NX_ASSERT(empty, "No space for new process"))
        return nullptr;

    *empty = {.pid = pid};
    return empty;
}

const SharedMemoryData::Session* SharedMemoryData::findSession(SessionId sessionId) const
{
    auto session = ranges::find_if(sessions,
        [&](const Session& session) { return session.id == sessionId; });

    return session != sessions.end() ? &(*session) : nullptr;
}

SharedMemoryData::Session* SharedMemoryData::findSession(SessionId sessionId)
{
    // It is safe to const-cast because current method is non-const.
    return const_cast<Session*>(
        static_cast<const SharedMemoryData*>(this)->findSession(sessionId));
}

SharedMemoryData::Session* SharedMemoryData::addSession(SessionId sessionId)
{
    NX_ASSERT(!findSession(sessionId), "Session %1 already exists", sessionId.toString());

    auto empty = findSession(SessionId());
    if (!NX_ASSERT(empty, "No space for new session"))
        return nullptr;

    *empty = {.id = sessionId};
    return empty;
}

SharedMemoryData::Session* SharedMemoryData::findProcessSession(PidType pid)
{
    auto process = findProcess(pid);
    return (process && process->sessionId != SessionId{})
        ? findSession(process->sessionId)
        : nullptr;
}

const SharedMemoryData::Session* SharedMemoryData::findProcessSession(PidType pid) const
{
    auto process = findProcess(pid);
    return process ? findSession(process->sessionId) : nullptr;
}

void PrintTo(SharedMemoryData::Command command, ::std::ostream* os)
{
    *os << nx::reflect::toString(command);
}

void PrintTo(SharedMemoryData::Process process, ::std::ostream* os)
{
    *os << nx::reflect::json::serialize(process);
}

void PrintTo(SharedMemoryData::Session session, ::std::ostream* os)
{
    *os << nx::reflect::json::serialize(session);
}

namespace shared_memory {

using Process = SharedMemoryData::Process;

PipeFilterType exceptPids(const QSet<SharedMemoryData::PidType>& pids)
{
    return ranges::views::filter(ProcessPredicate(
        [=](const SharedMemoryData::Process& process) { return !pids.contains(process.pid); }));
}

PipeFilterType withinSession(const SessionId& sessionId)
{
    return ranges::views::filter(ProcessPredicate(
        [=](const Process& process) { return process.sessionId == sessionId; }));
}

PipeFilterType all()
{
    return ranges::views::filter(ProcessPredicate([](const Process&) { return true; }));
}

} // namespace shared_memory

} // namespace nx::vms::client::desktop
