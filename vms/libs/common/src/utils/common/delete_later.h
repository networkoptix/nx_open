#ifndef QN_DELETE_LATER_H
#define QN_DELETE_LATER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "warnings.h"
#include "event_loop.h"

inline void qnDeleteLater(QObject *object) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    QThread *thread = object->thread();
    if(!thread) { 
        return; /* Application is being terminated. It's OK not to delete the object. */
    } else {
        object->deleteLater();

        if(!qnHasEventLoop(thread)) {
            qnCritical(
                "No event loop is running in thread %1@%2, to which the given object belongs. Object will not be deleted.", 
                thread->metaObject()->className(), 
                static_cast<void *>(thread)
            );
        }
    }
}


struct QScopedPointerLaterDeleter {
    static inline void cleanup(QObject *pointer) {
        qnDeleteLater(pointer);
    }
};


#endif // QN_DELETE_LATER_H
