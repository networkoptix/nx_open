// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QObject>
#include <QtCore/QThread>

#include <nx/utils/log/assert.h>

namespace nx::utils {

// Always deletes the QObject in the thread it belongs to (unless it's finished).
struct ThreadAffinityDeleter
{
    void operator()(QObject* o)
    {
        NX_ASSERT(!o->parent(), "QObject managed by a smart pointer must not have a parent");

        // Avoid manually managing the object deletion.
        std::unique_ptr<QObject> handle(o);

        const auto thread = o->thread();

        if (!thread || thread == QThread::currentThread())
            return;

        // Do things similar to deleteLater() but ensure object deletion in any case.
        if (auto dispatcher = QAbstractEventDispatcher::instance(o->thread()))
        {
            QMetaObject::invokeMethod(
                dispatcher,
                [handle = std::move(handle)]() mutable { handle.reset(); },
                Qt::QueuedConnection);
        }
    }
};

} // namespace nx::utils
