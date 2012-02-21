#ifndef QN_WEAK_GRAPHICS_ITEM_POINTER_H
#define QN_WEAK_GRAPHICS_ITEM_POINTER_H

#include <QWeakPointer>

class QGraphicsItem;

/**
 * This class presents functionality similar to that of a <tt>QWeakPointer</tt>,
 * but also works for non-<tt>QObject</tt> derived <tt>QGraphicsItem</tt>s.
 */
class WeakGraphicsItemPointer {
public:
    WeakGraphicsItemPointer();

    WeakGraphicsItemPointer(const WeakGraphicsItemPointer &other);

    WeakGraphicsItemPointer(QGraphicsItem *item);

    ~WeakGraphicsItemPointer();

    WeakGraphicsItemPointer &operator=(const WeakGraphicsItemPointer &other) {
        this->~WeakGraphicsItemPointer();
        new (this) WeakGraphicsItemPointer(other);
        return *this;
    }

    WeakGraphicsItemPointer &operator=(QGraphicsItem *item) {
        this->~WeakGraphicsItemPointer();
        new (this) WeakGraphicsItemPointer(item);
        return *this;
    }

    bool isNull() const {
        return m_guard.isNull();
    }

    QGraphicsItem *data() const {
        return isNull() ? NULL : m_item;
    }

    operator bool() const {
        return !isNull();
    }

    bool operator!() const {
        return isNull();
    }

    void clear() {
        m_guard.clear();
        m_item = NULL;
    }
    
private:
    QWeakPointer<QObject> m_guard;
    QGraphicsItem *m_item;
};

#endif // QN_WEAK_GRAPHICS_ITEM_POINTER_H
