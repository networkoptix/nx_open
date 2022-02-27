// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_info_storage.h"

#include <QMap>
#include <QStringList>

template<>
nx::vms::client::desktop::DebugInfoStorage*
    Singleton<nx::vms::client::desktop::DebugInfoStorage>::s_instance = nullptr;

namespace nx::vms::client::desktop {

class DebugInfoStorage::Private
{
    DebugInfoStorage* q;

public:
    QMap<QString, QString> storage;
    QStringList debugInfo;

public:
    Private(DebugInfoStorage* parent):
        q(parent)
    {
    }

    void updateDebugInfo()
    {
        debugInfo = storage.values();
        emit q->debugInfoChanged(debugInfo);
    }
};

DebugInfoStorage::DebugInfoStorage(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

DebugInfoStorage::~DebugInfoStorage()
{
}

const QStringList& DebugInfoStorage::debugInfo() const
{
    return d->debugInfo;
}

void DebugInfoStorage::setValue(const QString& key, const QString& value)
{
    d->storage[key] = value;
    d->updateDebugInfo();
}

void DebugInfoStorage::removeValue(const QString& key)
{
    if (d->storage.remove(key) > 0)
        d->updateDebugInfo();
}

} // namespace nx::vms::client::desktop
