#ifndef QN_TIME_STEP_H
#define QN_TIME_STEP_H

#include <QtCore/QtGlobal>

class QnTimeStep {
public:
    enum Type {
        Milliseconds,
        Days,
        Months,
        Years
    };

    QnTimeStep(): type(Milliseconds), unitMSecs(0), stepMSecs(0), stepUnits(0), wrapUnits(0), isRelative(true), index(0) {}

    QnTimeStep(Type type, qint64 unitMSecs, int stepUnits, int wrapUnits, const QString &format, const QString &longestString, const QString &longFormat, bool isRelative = true):
        type(type),
        unitMSecs(unitMSecs),
        stepMSecs(unitMSecs * stepUnits),
        stepUnits(stepUnits),
        wrapUnits(wrapUnits),
        format(format),
        longFormat(longFormat),
        longestString(longestString),
        isRelative(isRelative),
        index(0)
    {}

    /** Type of the time step. */
    Type type;

    /** Size of the unit in which step value is measured, in milliseconds. */
    qint64 unitMSecs;

    /** Time step, in milliseconds */
    qint64 stepMSecs;

    /** Time step, in units. */
    int stepUnits;

    /** Number of units for a wrap-around. */
    int wrapUnits;
        
    /** Format string for the step value. */
    QString format;

    /** Format string for the interval comment. */
    QString longFormat;

    /** Longest possible string representation of the step value. */
    QString longestString;

    /** Whether this time step is to be used for relative times (e.g. time intervals), 
     * or for absolute times (i.e. obtained via <tt>QDateTime::toMSecsSinceEpoch</tt>). */
    bool isRelative;

    /** Index of this time step in enclosing list. */
    int index;
};

qint64 roundUp(qint64 msecs, const QnTimeStep &step);

qint64 add(qint64 msecs, const QnTimeStep &step);

qint64 absoluteNumber(qint64 msecs, const QnTimeStep &step);

qint32 cacheKey(qint64 msecs, int height, const QnTimeStep &step);

QString toString(qint64 msecs, const QnTimeStep &step);

QString toLongString(qint64 msecs, const QnTimeStep &step);

#endif // QN_TIME_STEP_H
