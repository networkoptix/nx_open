#include "weak_graphics_item_pointer.h"
#include <QGraphicsItem>

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
