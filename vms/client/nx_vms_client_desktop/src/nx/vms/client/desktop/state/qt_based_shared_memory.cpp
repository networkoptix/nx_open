// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_based_shared_memory.h"

#include <QtCore/QDir>
#include <QtCore/QSharedMemory>

#include <nx/branding.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kDataVersion = 4;
static_assert(SessionId::kDataSize == 32, "Data version must be increased if changed.");
static_assert(SharedMemoryData::kClientCount == 64, "Data version must be increased if changed.");
static_assert(
    sizeof(SharedMemoryData::Command) == 4, "Data version must be increased if changed.");
static_assert(
    SharedMemoryData::kCommandDataSize == 64, "Data version must be increased if changed.");
static_assert(
    SharedMemoryData::kTokenSize == 8192, "Data version must be increased if changed.");

using ScreensBitMask = quint64;

using SerializedSessionId = std::array<char, SessionId::kDataSize>;

struct SerializedSharedMemoryProcessData
{
    /** Serialized representation of the session id. */
    SerializedSessionId sessionId;

    SharedMemoryData::PidType pid = 0;

    /** Bitmask of the used screens. */
    ScreensBitMask usedScreens = 0;

    SharedMemoryData::Command command = SharedMemoryData::Command::none;

    /** Serialized representation of the command data. */
    std::array<char, SharedMemoryData::kCommandDataSize> commandData;
};

struct SerializedSharedMemorySessionData
{
    SerializedSessionId id = {};
    std::array<char, SharedMemoryData::kTokenSize> token = {};
};

struct SerializedSharedMemoryData
{
    std::array<SerializedSharedMemoryProcessData, SharedMemoryData::kClientCount> processes;
    std::array<SerializedSharedMemorySessionData, SharedMemoryData::kClientCount> sessions;
};

ScreensBitMask serializedScreens(const SharedMemoryData::ScreenUsageData& screens)
{
    ScreensBitMask result = 0;
    for (int screen: screens)
        result |= 1ull << screen;
    return result;
}

SharedMemoryData::ScreenUsageData deserializedScreens(ScreensBitMask data)
{
    SharedMemoryData::ScreenUsageData screens;
    for (int index = 0; data; ++index)
    {
        if (data & 1)
            screens.insert(index);
        data = data >> 1;
    }
    return screens;
}

SerializedSharedMemoryProcessData serializedProcessData(const SharedMemoryData::Process& source)
{
    QByteArray sessionId = source.sessionId.serialized();

    SerializedSharedMemoryProcessData result;
    std::copy(sessionId.cbegin(), sessionId.cend(), result.sessionId.begin());
    result.pid = source.pid;
    result.usedScreens = serializedScreens(source.usedScreens);
    result.command = source.command;
    result.commandData.fill(0);
    QByteArray commandData = source.commandData;
    commandData.truncate(SharedMemoryData::kCommandDataSize);
    std::copy(commandData.cbegin(), commandData.cend(), result.commandData.begin());
    return result;
}

SerializedSharedMemorySessionData serializedSessionData(const SharedMemoryData::Session& session)
{
    SerializedSharedMemorySessionData result;
    const QByteArray sessionId = session.id.serialized();
    std::copy(sessionId.begin(), sessionId.end(), result.id.begin());

    NX_ASSERT(session.token.size() + /*null terminator*/ 1 <= SharedMemoryData::kTokenSize,
        "Invalid token size");

    std::copy(session.token.begin(), session.token.end(), result.token.begin());
    return result;
}

SharedMemoryData::Process deserializedProcessData(const SerializedSharedMemoryProcessData& source)
{
    SharedMemoryData::Process result;
    result.sessionId = SessionId::deserialized({source.sessionId.data(), SessionId::kDataSize});
    result.pid = source.pid;
    result.usedScreens = deserializedScreens(source.usedScreens);
    result.command = source.command;

    int commandDataSize = SharedMemoryData::kCommandDataSize;
    if (std::find(source.commandData.cbegin(), source.commandData.cend(), 0)
        != source.commandData.cend())
    {
        commandDataSize = -1; // Auto-detect QByteArray length as a null-terminated string.
    }
    result.commandData = QByteArray(source.commandData.data(), commandDataSize);
    return result;
}

SharedMemoryData::Session deserializedSessionData(const SerializedSharedMemorySessionData& source)
{
    SharedMemoryData::Session result;
    result.id = SessionId::deserialized({source.id.data(), SessionId::kDataSize});
    NX_ASSERT(source.token.back() == '\0');
    result.token = source.token.data();
    return result;
}

SharedMemoryData deserializedData(const SerializedSharedMemoryData& source)
{
    SharedMemoryData result;
    std::transform(source.processes.cbegin(), source.processes.cend(), result.processes.begin(),
        deserializedProcessData);
    std::transform(source.sessions.cbegin(), source.sessions.cend(), result.sessions.begin(),
        deserializedSessionData);
    return result;
}

void serializeData(const SharedMemoryData& source, SerializedSharedMemoryData* target)
{
    std::transform(source.processes.cbegin(), source.processes.cend(), target->processes.begin(),
        serializedProcessData);
    std::transform(source.sessions.cbegin(), source.sessions.cend(), target->sessions.begin(),
        serializedSessionData);
}

static const QString kDefaultSharedMemoryKey = nx::branding::customization()
    + "/DesktopClientQtBasedSharedMemory/" + QString::number(kDataVersion) + "/"
    + QFileInfo(QDir::homePath()).owner();

} // namespace

struct QtBasedSharedMemory::Private
{
    Private(const QString& explicitId):
        memory(explicitId.isEmpty() ? kDefaultSharedMemoryKey : explicitId)
    {
    }

    bool initialize()
    {
        bool success = memory.create(sizeof(SerializedSharedMemoryData));
        if (success)
        {
            memory.lock();
            setData({});
            memory.unlock();
        }

        if (!success && memory.error() == QSharedMemory::AlreadyExists)
            success = memory.attach();

        NX_ASSERT(success, "Could not acquire shared memory");
        return success;
    }

    SharedMemoryData data() const
    {
        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory.data());
        return deserializedData(*rawData);
    }

    void setData(const SharedMemoryData& data)
    {
        auto rawData = reinterpret_cast<SerializedSharedMemoryData*>(memory.data());
        serializeData(data, rawData);
    }

    mutable QSharedMemory memory;
};

QtBasedSharedMemory::QtBasedSharedMemory(const QString& explicitId):
    d(new Private(explicitId))
{
}

QtBasedSharedMemory::~QtBasedSharedMemory()
{
}

bool QtBasedSharedMemory::initialize()
{
    return d->initialize();
}

void QtBasedSharedMemory::lock()
{
    d->memory.lock();
}

void QtBasedSharedMemory::unlock()
{
    d->memory.unlock();
}

SharedMemoryData QtBasedSharedMemory::data() const
{
    return d->data();
}

void QtBasedSharedMemory::setData(const SharedMemoryData& data)
{
    d->setData(data);
}

} // namespace nx::vms::client::desktop
