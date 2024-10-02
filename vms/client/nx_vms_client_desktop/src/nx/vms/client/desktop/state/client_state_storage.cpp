// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_state_storage.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {

QString ClientStateStorage::kCommonSubstateFileName("auto_state.json");
QString ClientStateStorage::kSystemSubstateFileName("auto_state.json");
QString ClientStateStorage::kTemporaryStatesDirName("temp");

ClientStateStorage::ClientStateStorage(const QString& rootDirectory):
    m_root(rootDirectory)
{
}

SessionState ClientStateStorage::readCommonSubstate() const
{
    return readStateInternal("", kCommonSubstateFileName, /*mayFail*/ true);
}

void ClientStateStorage::writeCommonSubstate(const SessionState& commonState)
{
    writeStateInternal("", kCommonSubstateFileName, commonState);
}

SessionState ClientStateStorage::readSystemSubstate(const SessionId& sessionId) const
{
    return readStateInternal(sessionId, kSystemSubstateFileName, /*mayFail*/ true);
}

void ClientStateStorage::writeSystemSubstate(
    const SessionId& sessionId, const SessionState& systemState)
{
    writeStateInternal(sessionId.toQString(), kSystemSubstateFileName, systemState);
}

SessionState ClientStateStorage::takeTemporaryState(const QString& key)
{
    QDir directory(m_root);
    if (!directory.cd(kTemporaryStatesDirName))
        return {};

    auto path = directory.absoluteFilePath(key);
    auto [data, success] = readStateDirectly(path);

    if (NX_ASSERT(success, "Temporary file '%1' is not valid", key))
        directory.remove(key);

    return data;
}

void ClientStateStorage::putTemporaryState(const QString& key, const SessionState& state)
{
    writeStateInternal(kTemporaryStatesDirName, key, state);
}

void ClientStateStorage::clearSession(const SessionId& sessionId)
{
    QDir directory(m_root);
    if (!directory.cd(sessionId.toQString()))
        return;

    auto files = directory.entryList(QDir::Files, QDir::Name);
    for (const auto& name: files)
    {
        if (name != kSystemSubstateFileName)
            directory.remove(name);
    }
}

FullSessionState ClientStateStorage::readFullSessionState(const SessionId& sessionId) const
{
    const auto readState =
        [this](const QString& dirName) -> std::optional<FullSessionState>
        {
            QDir directory(m_root);
            if (!directory.cd(dirName))
                return std::nullopt;

            FullSessionState result;

            const QStringList files = directory.entryList(QDir::Files, QDir::Name);
            for (const QString& name: files)
            {
                if (name == kSystemSubstateFileName)
                    continue;

                const QString path = directory.absoluteFilePath(name);
                auto [data, success] = readStateDirectly(path);
                if (success)
                    result[name] = data;
            }

            return result;
        };

    const QString sessionIdString = sessionId.toQString();

    auto state = readState(sessionIdString);
    if (state)
        return *state;

    // If failed to read the state, try to read the state from the legacy directory name based on
    // the local system ID.
    const QString legacySessionIdString = sessionId.legacyIdString();

    if (sessionIdString != legacySessionIdString)
        state = readState(legacySessionIdString);

    return state.value_or(FullSessionState{});
}

SessionState ClientStateStorage::readSessionState(
    const SessionId& sessionId, const QString& key) const
{
    NX_ASSERT(key != kSystemSubstateFileName, "Full session restore should not use autosave files");
    return readStateInternal(sessionId, key, /*mayFail*/ false);
}

void ClientStateStorage::writeSessionState(
    const SessionId& sessionId, const QString& key, const SessionState& state)
{
    NX_ASSERT(key != kSystemSubstateFileName, "Full session restore should not use autosave files");
    writeStateInternal(sessionId.toQString(), key, state);
}

SessionState ClientStateStorage::readStateInternal(
    const SessionId& sessionId, const QString& name, bool mayFail) const
{
    const QString sessionIdString = sessionId.toQString();
    const QString legacySessionIdString = sessionId.legacyIdString();
    const bool mayTryLegacyId = (sessionIdString != legacySessionIdString);

    const SessionState state = readStateInternal(sessionIdString, name, mayFail || mayTryLegacyId);
    if (!state.isEmpty() || !mayTryLegacyId)
        return state;

    // If failed to read the state, try to read the state from the legacy directory name based on
    // the local system ID.
    return readStateInternal(legacySessionIdString, kSystemSubstateFileName, mayFail);
}

SessionState ClientStateStorage::readStateInternal(
    const QString& dir,
    const QString& name,
    bool mayFail) const
{
    QDir directory(m_root);
    if (dir != "" && dir != ".")
    {
        if (!directory.cd(dir))
        {
            NX_ASSERT(mayFail, "Directory '%1' cannot be read", dir);
            return {};
        }
    }

    const auto path = directory.absoluteFilePath(name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_ASSERT(mayFail, "File '%1' cannot be opened for reading", path);
        return {};
    }

    auto result = QJson::deserialized(file.readAll(), SessionState{});

    file.close();
    return result;
}

void ClientStateStorage::writeStateInternal(
    const QString& dir,
    const QString& name,
    const SessionState& state)
{
    if (!m_root.exists())
        m_root.mkpath(".");

    QDir directory(m_root);
    if (dir != "" && dir != ".")
    {
        directory.mkdir(dir);
        if (!NX_ASSERT(directory.cd(dir), "Directory '%1' cannot be opened", dir))
            return;
    }

    const auto path = directory.absoluteFilePath(name);
    QFile file(path);
    if (!NX_ASSERT(file.open(QIODevice::WriteOnly), "File '%1' cannot be opened for writing", path))
        return;

    file.write(QJson::serialized(state));
    file.close();
}

std::pair<SessionState, bool> ClientStateStorage::readStateDirectly(const QString& path) const
{
    QFile file(path);
    if (!NX_ASSERT(file.open(QIODevice::ReadOnly), "File '%1' cannot be opened for direct reading", path))
        return {{}, false};

    bool success = true;
    auto result = QJson::deserialized(file.readAll(), SessionState{}, &success);

    file.close();
    return {result, success};
}

} // namespace nx::vms::client::desktop
