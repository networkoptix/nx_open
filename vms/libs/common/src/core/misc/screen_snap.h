#pragma once

#include <array>

#include <QtCore/QSet>
#include <QtCore/QRect>
#include <QtCore/QDebug>

struct QnScreenSnap
{
    int screenIndex = -1;   /**< Index of the screen. */
    int snapIndex = 0;      /**< Index of the snap on the screen. */

    QnScreenSnap();
    QnScreenSnap(int screenIndex, int snapIndex);

    bool isValid() const;
    int encode() const;
    static QnScreenSnap decode(int encoded);
    static int snapsPerScreen();

    bool operator==(const QnScreenSnap& other) const
    {
        return (screenIndex == other.screenIndex && snapIndex == other.snapIndex);
    }

    bool operator!=(const QnScreenSnap& other) const
    {
        return !(*this == other);
    }
};

struct QnScreenSnaps
{
    QnScreenSnap& left() { return values[0]; }
    const QnScreenSnap& left() const { return values[0]; }

    QnScreenSnap& right() { return values[1]; }
    const QnScreenSnap& right() const { return values[1]; }

    QnScreenSnap& top() { return values[2]; }
    const QnScreenSnap& top() const { return values[2]; }

    QnScreenSnap& bottom() { return values[3]; }
    const QnScreenSnap& bottom() const { return values[3]; }

    std::array<QnScreenSnap, 4> values;

    bool isValid() const;

    QRect geometry(const QList<QRect>& screenGeometries) const;

    bool operator==(const QnScreenSnaps& other) const
    {
        return values == other.values;
    }

    bool operator!=(const QnScreenSnaps& other) const
    {
        return !(*this == other);
    }
};

QDebug operator<<(QDebug dbg, const QnScreenSnap& snap);
QDebug operator<<(QDebug dbg, const QnScreenSnaps& snaps);
