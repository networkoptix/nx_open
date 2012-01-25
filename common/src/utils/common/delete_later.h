#ifndef QN_DELETE_LATER_H
#define QN_DELETE_LATER_H

#include <QObject>

#ifndef QN_NO_DELETE_LATER_DEBUG
#   include <QThread>
#   include <QtCore/private/qthread_p.h>
#   include "warnings.h"
#endif

inline void qnDeleteLater(QObject *object) {
    object->deleteLater();

#ifndef QN_NO_DELETE_LATER_DEBUG
    int loopLevel = QThreadData::get2(object->thread())->loopLevel;
    if(loopLevel == 0)
        qnCritical("No event loop is running in given object's thread. Object will not be deleted.");
#endif
}


#endif // QN_DELETE_LATER_H
