#ifndef QN_MARGINS_H
#define QN_MARGINS_H

#include <QtGlobal>
#include <QMargins>

class MarginsF {
public:
    MarginsF() {
        m_top = m_bottom = m_left = m_right = 0.0;
    }

    MarginsF(qreal left, qreal top, qreal right, qreal bottom): 
        m_left(left), m_top(top), m_right(right), m_bottom(bottom) 
    {}

    MarginsF(const QMargins &other):
        m_left(other.left()), m_top(other.top()), m_right(other.right()), m_bottom(other.bottom()) 
    {}

    bool isNull() const {
        return m_left == 0.0 && m_top == 0.0 && m_right == 0.0 && m_bottom == 0.0;
    }

    qreal left() const {
        return m_left;
    }

    qreal top() const {
        return m_top;
    }

    qreal right() const {
        return m_right;
    }

    qreal bottom() const {
        return m_bottom;
    }

    void setLeft(qreal left) {
        m_left = left;
    }

    void setTop(qreal top) {
        m_top = top;
    }

    void setRight(qreal right) {
        m_right = right;
    }

    void setBottom(qreal bottom) {
        m_bottom = bottom;
    }

    QMargins toMargins() const {
        return QMargins(m_left, m_top, m_right, m_bottom);
    }

    friend inline bool operator==(const MarginsF &l, const MarginsF &r) {
        return
            l.m_left   == r.m_left &&
            l.m_top    == r.m_top &&
            l.m_right  == r.m_right &&
            l.m_bottom == r.m_bottom;
    }

    friend inline bool operator!=(const MarginsF &l, const MarginsF &r) {
        return
            l.m_left   != r.m_left ||
            l.m_top    != r.m_top ||
            l.m_right  != r.m_right ||
            l.m_bottom != r.m_bottom;
    }

private:
    qreal m_left;
    qreal m_top;
    qreal m_right;
    qreal m_bottom;
};

Q_DECLARE_TYPEINFO(MarginsF, Q_MOVABLE_TYPE);

#endif // QN_MARGINS_H
