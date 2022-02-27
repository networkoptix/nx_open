// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_DELETE_LATER_H
#define QN_DELETE_LATER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "event_loop.h"

inline void qnDeleteLater(QObject *object)
{
    if (!NX_ASSERT(object))
        return;

    QThread *thread = object->thread();
    if(!thread) {
        return; /* Application is being terminated. It's OK not to delete the object. */
    } else {
        object->deleteLater();

        NX_ASSERT(
            qnHasEventLoop(thread),
            "No event loop is running in thread %1@%2, to which the given object belongs. Object will not be deleted.",
            thread->metaObject()->className(),
            static_cast<void *>(thread));
    }
}

#endif // QN_DELETE_LATER_H
