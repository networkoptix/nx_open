// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_info_storage.h"

#include <QMap>
#include <QStringList>

#include <nx/reflect/to_string.h>
#include <nx/vms/client/desktop/ini.h>

template<>
nx::vms::client::desktop::DebugInfoStorage*
    Singleton<nx::vms::client::desktop::DebugInfoStorage>::s_instance = nullptr;

namespace nx::vms::client::desktop {

QString toString(DebugInfoStorage::Field field)
{
    return QString::fromStdString(nx::reflect::toString(field));
}

class DebugInfoStorage::Private
{
    DebugInfoStorage* q;

public:
    QMap<QString, QString> storage;
    QStringList debugInfo;
    std::set<DebugInfoStorage::Field> filter;

public:
    Private(DebugInfoStorage* parent):
        q(parent)
    {
        const QString filterList(ini().debugInfoFilter);
        const auto addFieldToFilter = [filterList, this](const DebugInfoStorage::Field& key)
        {
            if (filterList.contains(toString(key)))
                filter.insert(key);
        };
        for (auto i = static_cast<int>(DebugInfoStorage::Field::all);
             i < static_cast<int>(DebugInfoStorage::Field::last);
             ++i)
        {
            addFieldToFilter(static_cast<DebugInfoStorage::Field>(i));
        }
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

const std::set<DebugInfoStorage::Field>& DebugInfoStorage::filter() const
{
    return d->filter;
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
