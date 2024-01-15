// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QTimeZone>
#include <QtCore/QtGlobal>

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

    QnTimeStep() = default;

    QnTimeStep(Type type,
        milliseconds unitMSecs,
        int stepUnits,
        int wrapUnits,
        const QString& format,
        const QString& longFormat,
        bool isRelative = true);

    /** Type of the time step. */
    Type type = Milliseconds;

    /** Size of the unit in which step value is measured, in milliseconds. */
    milliseconds unitMSecs = milliseconds(0);

    /** Time step, in milliseconds */
    milliseconds stepMSecs = milliseconds(0);

    /** Time step, in units. */
    int stepUnits = 0;

    /** Number of units for a wrap-around. */
    int wrapUnits = 0;

    /** Format string for the step value. */
    QString format;

    /** Format string for the interval comment. */
    QString longFormat;

    /** Whether this time step is to be used for relative times (e.g. time intervals),
     * or for absolute times (i.e. obtained via <tt>QDateTime::toMSecsSinceEpoch</tt>). */
    bool isRelative = true;

    /** Index of this time step in the enclosing list. */
    int index = 0;
};

// We do not want chrono::milliseconds propagate to hashes etc., so we leave qint64..
typedef QPair<qint64, qint64> QnTimeStepLongCacheKey;

// These function are used in low-level drawings, so we return qint64 instead of time.
qint64 roundUp(std::chrono::milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone);
qint64 add(std::chrono::milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone);
qint64 sub(std::chrono::milliseconds msecs, const QnTimeStep& step, const QTimeZone& timeZone);

qint64 absoluteNumber(
    std::chrono::milliseconds msecs,
    const QnTimeStep& step,
    const QTimeZone& timeZone);

qint32 shortCacheKey(
    std::chrono::milliseconds msecs,
    int height,
    const QnTimeStep& step,
    const QTimeZone& timeZone);

QnTimeStepLongCacheKey longCacheKey(
    std::chrono::milliseconds msecs,
    int height,
    const QnTimeStep& step);

// TODO: #sivanov What to do with locale-translation inconsistencies?

// Used for time label below ticks (both smaller and bigger).
QString toShortString(
    std::chrono::milliseconds msecs,
    const QnTimeStep& step,
    const QTimeZone& timeZone);

QString toLongestShortString(const QnTimeStep& step);

// Used for upper time labels in rectangles.
QString toLongString(
    std::chrono::milliseconds msecs,
    const QnTimeStep& step,
    const QTimeZone& timeZone);
