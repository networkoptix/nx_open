#include "weak_graphics_item_pointer.h"

#include <iterator> /* For std::back_inserter. */
#include <algorithm> /* For std::copy. */

#include <QtWidgets/QGraphicsItem>

namespace {

    class GuardItem: public QGraphicsObject {
    public:
        GuardItem(QGraphicsItem *parent = NULL): 
            QGraphicsObject(parent)
        {
            setFlags(ItemHasNoContents);
            setAcceptedMouseButtons(0);
            setEnabled(false);
            setVisible(false);
        }

        virtual QRectF boundingRect() const override {
            return QRectF();
        }

    protected:
        virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {
            return;
        }

    };

    enum {
        GUARD_KEY = 0x49513299
    };

} // anonymous namespace


// -------------------------------------------------------------------------- //
// WeakGraphicsItemPointer
// -------------------------------------------------------------------------- //
WeakGraphicsItemPointer::WeakGraphicsItemPointer():
    m_item(NULL)
{}

WeakGraphicsItemPointer::WeakGraphicsItemPointer(const WeakGraphicsItemPointer &other) {
    m_guard = other.m_guard;
    m_item = other.m_item;
}

WeakGraphicsItemPointer::WeakGraphicsItemPointer(QGraphicsItem *item):
    m_item(item)
{
    if(item == NULL)
        return;

    QGraphicsObject *object = item->toGraphicsObject();
    if(object != NULL) {
        m_guard = object;
        return;
    }
    
    QObject *guard = item->data(GUARD_KEY).value<QObject *>();
    if(guard == NULL) {
        guard = new GuardItem(item);
        item->setData(GUARD_KEY, QVariant::fromValue<QObject *>(guard));
    }
    m_guard = guard;
}

WeakGraphicsItemPointer::~WeakGraphicsItemPointer() {
    m_item = NULL;
}


// -------------------------------------------------------------------------- //
// WeakGraphicsItemPointerList
// -------------------------------------------------------------------------- //
WeakGraphicsItemPointerList::WeakGraphicsItemPointerList(const QList<QGraphicsItem *> &items) {
    reserve(items.size());

    std::copy(items.begin(), items.end(), std::back_inserter(*this));
}

void WeakGraphicsItemPointerList::compress() {
    for(iterator pos = begin(); pos != end();) {
        if(*pos) {
            ++pos;
        } else {
            pos = erase(pos);
        }
    }
}

WeakGraphicsItemPointerList WeakGraphicsItemPointerList::compressed() const {
    WeakGraphicsItemPointerList result = *this;
    result.compress();
    return result;
}

QList<QGraphicsItem *> WeakGraphicsItemPointerList::materialized() const {
    QList<QGraphicsItem *> result;
    for(const_iterator pos = begin(), end = this->end(); pos != end; pos++)
        if(*pos)
            result.push_back(pos->data());
    return result;
}

int WeakGraphicsItemPointerList::indexOf(QGraphicsItem *value, int from) const {
    if (from < 0)
        from = qMax(from + size(), 0);

    if (from < size())
        for(const_iterator pos = begin() + from, end = this->end(); pos != end; pos++)
            if (pos->data() == value)
                return pos - begin();;

    return -1;
}

int WeakGraphicsItemPointerList::lastIndexOf(QGraphicsItem *value, int from) const
{
    if (from < 0) {
        from += size();
    } else if (from >= size()) {
        from = size() - 1;
    }

    if (from >= 0) 
        for(const_iterator pos = begin() + from, end = this->end(); pos != end; pos--)
            if(pos->data() == value)
                return pos - begin();

    return -1;
}

bool WeakGraphicsItemPointerList::contains(QGraphicsItem *value) const {
    return indexOf(value) != -1;
}

int WeakGraphicsItemPointerList::removeAll(QGraphicsItem *value) {
    if(isEmpty())
        return 0;

    detach();

    int result = 0;
    for(iterator pos = begin(); pos != end();) {
        if(pos->data() == value) {
            pos = erase(pos);
            result++;
        } else {
            pos++;
        }
    }

    return result;
}

bool WeakGraphicsItemPointerList::removeOne(QGraphicsItem *value) {
    if(isEmpty())
        return false;

    detach();

    int index = indexOf(value);
    if (index != -1) {
        remove(index);
        return true;
    }
    
    return false;
}


