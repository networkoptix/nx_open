// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

/**
 * Utility logic layer for objects, which can be modified locally and remotely simultaneously.
 * It's main purpose is to avoid overriding unsaved local changes when remote data is received.
 */
class NX_VMS_CLIENT_CORE_API SaveStateManager: public QObject
{
    Q_OBJECT
    Q_FLAGS(SaveStateFlags)

    using base_type = QObject;

public:
    enum SaveStateFlag
    {
        IsBeingSaved = 0x1, //< Is currently being saved to the server.
        IsChanged = 0x2,    //< Unsaved changes are present.
    };
    Q_DECLARE_FLAGS(SaveStateFlags, SaveStateFlag)

    SaveStateManager(QObject* parent = nullptr);

    SaveStateFlags flags(const nx::Uuid& id) const;
    void setFlags(const nx::Uuid& id, SaveStateFlags flags);

    void clean(const nx::Uuid& id);

    bool isBeingSaved(const nx::Uuid& id) const;
    bool isChanged(const nx::Uuid& id) const;

    /**
     * Check if item is changed and is not being saved right now.
     */
    bool isSaveable(const nx::Uuid& id) const;

    void markBeingSaved(const nx::Uuid& id, bool saved);
    void markChanged(const nx::Uuid& id, bool changed);

    bool hasSaveRequests(const nx::Uuid& id) const;
    void addSaveRequest(const nx::Uuid& id, int reqId);
    void removeSaveRequest(const nx::Uuid& id, int reqId);

signals:
    void flagsChanged(const nx::Uuid& id, SaveStateFlags flags);

private:
    mutable nx::Mutex m_mutex;
    QHash<nx::Uuid, SaveStateFlags> m_flags;
    QHash<nx::Uuid, QSet<int>> m_saveRequests;
};

} // namespace nx::vms::client::core
