#ifndef QN_TIME_STEP_H
#define QN_TIME_STEP_H

#include <chrono>

#include <QtCore/QtGlobal>
#include <QtCore/QPair>
#include <QtCore/QString>

class QnTimeStep
{
    using milliseconds = std::chrono::milliseconds;
public:
    enum Type
    {
        Milliseconds,
        Seconds,
        Minutes,
        Hours,
        Days,
        Months,
        Years
    };

    QnTimeStep();

    QnTimeStep(Type type, milliseconds unitMSecs, int stepUnits, int wrapUnits,
        const QString& format, const QString& longFormat, bool isRelative = true);

    /** Type of the time step. */
    Type type;

    /** Size of the unit in which step value is measured, in milliseconds. */
    milliseconds unitMSecs;

    /** Time step, in milliseconds */
    milliseconds stepMSecs;

    /** Time step, in units. */
    int stepUnits;

    /** Number of units for a wrap-around. */
    int wrapUnits;

    /** Format string for the step value. */
    QString format;

    /** Format string for the interval comment. */
    QString longFormat;

    /** Whether this time step is to be used for relative times (e.g. time intervals),
     * or for absolute times (i.e. obtained via <tt>QDateTime::toMSecsSinceEpoch</tt>). */
    bool isRelative;

    /** Index of this time step in the enclosing list. */
    int index;
};

// We do not want chrono::milliseconds propagate to hashes etc., so we leave qint64..
typedef QPair<qint64, qint64> QnTimeStepLongCacheKey;

// These function are used in low-level drawings, so we return qint64 instead of time.
qint64 roundUp(std::chrono::milliseconds msecs, const QnTimeStep &step);
qint64 add(std::chrono::milliseconds msecs, const QnTimeStep &step);
qint64 sub(std::chrono::milliseconds msecs, const QnTimeStep &step);
qint64 absoluteNumber(std::chrono::milliseconds msecs, const QnTimeStep &step);

qint32 shortCacheKey(std::chrono::milliseconds msecs, int height, const QnTimeStep &step);

QnTimeStepLongCacheKey longCacheKey(std::chrono::milliseconds msecs, int height, const QnTimeStep &step);

// TODO: #Elric #TR what to do with locale-translation inconsistencies?

QString toShortString(std::chrono::milliseconds msecs, const QnTimeStep &step);

QString toLongestShortString(const QnTimeStep &step);

QString toLongString(std::chrono::milliseconds msecs, const QnTimeStep &step);

#endif // QN_TIME_STEP_H
