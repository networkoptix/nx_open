#ifndef QN_DELETE_LATER_H
#define QN_DELETE_LATER_H

#include <QObject>

#ifndef QN_NO_DELETE_LATER_DEBUG
#   include <QThread>
#   include <QtCore/private/qthread_p.h>
#   include "warnings.h"
#endif

inline void qnDeleteLater(QObject *object) {
    QThread *thread = object->thread();
    if(thread == NULL) { /* Application is being terminated. */
        delete object;
    } else {
        object->deleteLater();

#ifndef QN_NO_DELETE_LATER_DEBUG
        int loopLevel = QThreadData::get2(thread)->loopLevel;
        if(loopLevel == 0) {
            qnCritical(
                "No event loop is running in thread %1@%2, to which the given object belongs. Object will not be deleted.", 
                object->thread()->metaObject()->className(), 
                static_cast<void *>(object->thread())
            );
        }
#endif
    }
}


#endif // QN_DELETE_LATER_H
