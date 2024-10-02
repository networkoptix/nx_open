// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

#include <QtCore/QDir>

#include <nx/vms/client/desktop/state/session_id.h>

#include "session_state.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ClientStateStorage
{
public:
    static QString kCommonSubstateFileName; //< Used to store system-independent parameters.
    static QString kSystemSubstateFileName; //< Used to store system-specific parameters.
    static QString kTemporaryStatesDirName; //< Used to store inherited states during client start.

    explicit ClientStateStorage(const QString& rootDirectory);

    /**
     * Read system-independent parameters from the storage.
     * These parameters are intended for the automatic ('Soft') session restore.
     */
    SessionState readCommonSubstate() const;

    /**
     * Save system-independent parameters into the storage.
     */
    void writeCommonSubstate(const SessionState& commonState);

    /**
     * Read system-dependent parameters from the storage.
     * These parameters are intended for the automatic ('Soft') session restore.
     */
    SessionState readSystemSubstate(const SessionId& sessionId) const;

    /**
     * Save system-dependent parameters into the storage.
     */
    void writeSystemSubstate(const SessionId& sessionId, const SessionState& systemState);

    /**
     * Read temporary state from the storage.
     * Key must be a valid file name. This file is deleted after reading(!).
     */
    SessionState takeTemporaryState(const QString& key);

    /**
     * Save temporary state.
     * Key must be a valid file name.
     */
    void putTemporaryState(const QString& key, const SessionState& state);

    /**
     * Remove all the session data from the storage.
     */
    void clearSession(const SessionId& sessionId);

    /**
     * Read session state of all windows of the given session at once.
     * This method as well may be used to find out key values existing in the session.
     */
    FullSessionState readFullSessionState(const SessionId& sessionId) const;

    /**
     * Read session state of a single window.
     * Key must be a valid file name, it should exist in the session.
     */
    SessionState readSessionState(const SessionId& sessionId, const QString& key) const;

    /**
     * Save session state of a single window.
     * Key must be a valid file name, it should not be already used.
     */
    void writeSessionState(
        const SessionId& sessionId, const QString& key, const SessionState& state);

private:
    SessionState readStateInternal(
        const SessionId& sessionId, const QString& name, bool mayFail) const;
    SessionState readStateInternal(const QString& dir, const QString& name, bool mayFail) const;
    void writeStateInternal(const QString& dir, const QString& name, const SessionState& state);

    // Used for batch file processing.
    std::pair<SessionState, bool> readStateDirectly(const QString& path) const;

private:
    QDir m_root;
};

} // namespace nx::vms::client::desktop
