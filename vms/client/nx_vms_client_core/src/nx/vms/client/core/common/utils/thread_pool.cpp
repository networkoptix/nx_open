// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thread_pool.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::core {

ThreadPool::ThreadPool(const QString& id)
{
    NX_VERBOSE(this, "Creating thread pool %1 (%2)", this, id);
    setObjectName(id);
}

ThreadPool::~ThreadPool()
{
    NX_VERBOSE(this, "Destroying thread pool %1 (%2)", this, objectName());
}

ThreadPool* ThreadPool::instance(const QString& id)
{
    return appContext()->threadPool(id);
}

} // namespace nx::vms::client::core
