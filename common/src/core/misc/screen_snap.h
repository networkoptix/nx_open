#ifndef QN_SCREEN_SNAP_H
#define QN_SCREEN_SNAP_H

#include <QtCore/QMetaType>

struct QnScreenSnap {
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

struct QnScreenSnaps {
    QnScreenSnap left;
    QnScreenSnap right;
    QnScreenSnap top;
    QnScreenSnap bottom;

    bool isValid() const;

    QSet<int> screens() const;

    friend bool operator==(const QnScreenSnaps &l, const QnScreenSnaps &r) {
        return (l.left == r.left &&
            l.right == r.right &&
            l.top == r.top &&
            l.bottom == r.bottom);
    }
};

#endif // QN_SCREEN_SNAP_H
