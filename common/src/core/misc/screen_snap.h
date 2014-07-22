#ifndef QN_SCREEN_SNAP_H
#define QN_SCREEN_SNAP_H

#include <QtCore/QMetaType>

#include <array>

#include <boost/operators.hpp>

struct QnScreenSnap: public boost::equality_comparable1<QnScreenSnap>  {
    int screenIndex;    /**< Index of the screen. */
    int snapIndex;      /**< Index of the snap on the screen. */

    QnScreenSnap();
    QnScreenSnap(int screenIndex, int snapIndex);

    bool isValid() const;
    int encode() const;
    static QnScreenSnap decode(int encoded);
    static int snapsPerScreen();

    friend bool operator==(const QnScreenSnap &l, const QnScreenSnap &r) {
        return (l.screenIndex == r.screenIndex &&
            l.snapIndex == r.snapIndex);
    }
};

struct QnScreenSnaps: public boost::equality_comparable1<QnScreenSnaps> {
    inline QnScreenSnap &left() {return values[0];}
    inline const QnScreenSnap &left() const {return values[0];}

    inline QnScreenSnap &right() {return values[1];}
    inline const QnScreenSnap &right() const {return values[1];}

    inline QnScreenSnap &top() {return values[2];}
    inline const QnScreenSnap &top() const {return values[2];}

    inline QnScreenSnap &bottom() {return values[3];}
    inline const QnScreenSnap &bottom() const {return values[3];}

    std::array<QnScreenSnap, 4> values;

    bool isValid() const;

    QSet<int> screens() const;

    QRect geometry(const QList<QRect> &screens) const;

    friend bool operator==(const QnScreenSnaps &l, const QnScreenSnaps &r) {
        return l.values == r.values;
    }
};

#endif // QN_SCREEN_SNAP_H
