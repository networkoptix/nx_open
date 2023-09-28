// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WEAK_GRAPHICS_ITEM_POINTER_H
#define QN_WEAK_GRAPHICS_ITEM_POINTER_H

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QWeakPointer>

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
        return isNull() ? nullptr : m_item;
    }

    operator bool() const {
        return !isNull();
    }

    bool operator!() const {
        return isNull();
    }

    void clear() {
        m_guard.clear();
        m_item = nullptr;
    }

private:
    QPointer<QObject> m_guard;
    QGraphicsItem *m_item;
};


class WeakGraphicsItemPointerList: public QVector<WeakGraphicsItemPointer> {
    typedef QVector<WeakGraphicsItemPointer> base_type;

public:
    WeakGraphicsItemPointerList() {}
    WeakGraphicsItemPointerList(int size): base_type(size) {}
    WeakGraphicsItemPointerList(int size, const WeakGraphicsItemPointer &value): base_type(size, value) {}
    WeakGraphicsItemPointerList(const QList<QGraphicsItem *> &items);

    /**
     * Compressed this list by removing all null items.
     */
    void compress();

    /**
     * \returns                         A copy of this list with all null items removed.
     */
    WeakGraphicsItemPointerList compressed() const;

    /**
     * \returns                         A list of graphics items that this list contains,
     *                                  excluding null items and items that were already
     *                                  destroyed.
     */
    QList<QGraphicsItem *> materialized() const;

    using base_type::indexOf;
    using base_type::lastIndexOf;
    using base_type::contains;

    int indexOf(QGraphicsItem *value, int from = 0) const;

    int lastIndexOf(QGraphicsItem *value, int from = -1) const;

    bool contains(QGraphicsItem *value) const;

    int removeAll(QGraphicsItem *value);

    bool removeOne(QGraphicsItem *value);
};

#endif // QN_WEAK_GRAPHICS_ITEM_POINTER_H
