// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/thread/mutex.h>

#include "../thread_pool.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ThreadPool::Manager: public QObject
{
public:
    explicit Manager(QObject* parent = nullptr);
    virtual ~Manager() override;

    ThreadPool* get(const QString& id);

private:
    QHash<QString, QPointer<ThreadPool>> m_threadPools;
    bool m_destroyed = false;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::vms::client::core
