#ifndef QN_DELETE_LATER_H
#define QN_DELETE_LATER_H

#include <QtCore/QObject>
#include "warnings.h"

#ifdef QN_HAS_PRIVATE_INCLUDES
#   include <QtCore/QThread>
#   include <QtCore/private/qthread_p.h>
#endif

inline void qnDeleteLater(QObject *object) {
    if(object == NULL) {
        qnNullWarning(object);
        return;
    }

    QThread *thread = object->thread();
    if(thread == NULL) { 
        return; /* Application is being terminated. It's OK not to delete it. */
    } else {
        object->deleteLater();

#ifdef QN_HAS_PRIVATE_INCLUDES
        int loopLevel = QThreadData::get2(thread)->loopLevel;
        if(loopLevel == 0) {
            /*qnCritical(
                "No event loop is running in thread %1@%2, to which the given object belongs. Object will not be deleted.", 
                object->thread()->metaObject()->className(), 
                static_cast<void *>(object->thread())
            );*/
        }
#endif
    }
}


#endif // QN_DELETE_LATER_H
