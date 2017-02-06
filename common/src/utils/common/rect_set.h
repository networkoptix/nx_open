#ifndef QN_RECT_SET_H
#define QN_RECT_SET_H

#include <QtCore/QRect>
#include <QtCore/QPoint>
#include <set>
#include "warnings.h"

/**
 * Rectangle set is an abstraction of a set of rectangles with fast
 * insertion, deletion and bounding rectangle calculation.
 */
class QnRectSet {
public:
    QnRectSet() {}

    void clear() {
        m_xSet.clear();
        m_ySet.clear();
    }

    bool empty() const {
        /* Only one set may be empty because of inconsistent removals. */
        return m_xSet.empty() || m_ySet.empty();
    }

    int size() const {
        return (int) m_xSet.size() / 2;
    }

    void insert(const QRect &rect) {
        if (!rect.isValid())
            return;

        m_xSet.insert(rect.left());
        m_xSet.insert(rect.right());
        m_ySet.insert(rect.top());
        m_ySet.insert(rect.bottom());
    }

    void remove(const QRect &rect) {
        if (!rect.isValid())
            return;

        remove(m_xSet, rect.left());
        remove(m_xSet, rect.right());
        remove(m_ySet, rect.top());
        remove(m_ySet, rect.bottom());
    }

    QRect boundingRect() const {
        if(empty())
            return QRect();

        return QRect(
            QPoint(
                *m_xSet.begin(),
                *m_ySet.begin()
            ),
            QPoint(
                *prev(m_xSet.end()),
                *prev(m_ySet.end())
            )
        );
    }

private:
    void remove(std::multiset<int> &set, int value) {
        std::multiset<int>::iterator pos = set.find(value);
        if(pos == set.end()) {
            qnWarning("Given rectangle does not belong to this rectangle set, rectangle set may become inconsistent.");
            return;
        }

        set.erase(pos);
    }

    template<class Iterator>
    Iterator prev(const Iterator &pos) const {
        Iterator result = pos;
        result--;
        return result;
    }

private:
    std::multiset<int> m_xSet, m_ySet;
};

#endif // QN_RECT_SET_H
