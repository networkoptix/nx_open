#ifndef QN_SCREEN_SNAP_H
#define QN_SCREEN_SNAP_H

#include <QtCore/QMetaType>

#include <algorithm>

struct QnScreenSnap {
    int screenIndex;    /**< Index of the screen. */
    int snapIndex;      /**< Index of the snap on the screen. */

    QnScreenSnap(): screenIndex(-1), snapIndex(0) {}

    QnScreenSnap(int screenIndex, int snapIndex):
        screenIndex(screenIndex), snapIndex(snapIndex) {}

    bool isValid() const {
        return screenIndex >= 0;
    }

    int encode() const {
        return (screenIndex << 8) | (snapIndex & 0xFF);
    }

    static QnScreenSnap decode(int encoded) {
        return QnScreenSnap(encoded >> 8, encoded & 0xFF);
    }
};

struct QnScreenSnaps {
    QnScreenSnap left;
    QnScreenSnap right;
    QnScreenSnap top;
    QnScreenSnap bottom;
};

/*
struct QnScreenSnaps {
    std::vector<QnScreenSnap> left;
    std::vector<QnScreenSnap> right;
    std::vector<QnScreenSnap> top;
    std::vector<QnScreenSnap> bottom;

    QnScreenSnaps filtered(int screenIndex) const {
        auto filter = [screenIndex](const QnScreenSnap &snap) {return snap.screenIndex == screenIndex; };

        QnScreenSnaps result;
        std::copy_if(left.cbegin(),     left.cend(),    std::back_inserter(result.left),    filter);
        std::copy_if(right.cbegin(),    right.cend(),   std::back_inserter(result.right),   filter);
        std::copy_if(top.cbegin(),      top.cend(),     std::back_inserter(result.top),     filter);
        std::copy_if(bottom.cbegin(),   bottom.cend(),  std::back_inserter(result.bottom),  filter);
        return result;
    }
  
    QnScreenSnaps joined() const {
        QnScreenSnaps result;

        auto contains = [](const QList<QnScreenSnap> &list, const int value) {
            foreach (const QnScreenSnap &snap, list)
                if (snap.value == value)
                    return true;
            return false;
        };

        foreach (const QnScreenSnap &i, left)    if (!contains(result.left, i.value)) result.left << i;
        foreach (const QnScreenSnap &i, right)   if (!contains(result.right, i.value)) result.right << i;
        foreach (const QnScreenSnap &i, top)     if (!contains(result.top, i.value)) result.top << i;
        foreach (const QnScreenSnap &i, bottom)  if (!contains(result.bottom, i.value)) result.bottom << i;
        return result;
    }
};*/



#endif // QN_SCREEN_SNAP_H
