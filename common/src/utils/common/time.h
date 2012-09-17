#ifndef QN_TIME_H
#define QN_TIME_H

#include <QtCore/QTime>

inline qint64 timeToMSecs(const QTime &time) {
    return QTime(0, 0, 0, 0).msecsTo(time);
}

inline QTime msecsToTime(qint64 msecs) {
    return QTime(0, 0, 0, 0).addMSecs(msecs); 
}


#endif // QN_TIME_H
