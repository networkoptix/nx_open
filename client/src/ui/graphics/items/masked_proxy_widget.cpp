#include "masked_proxy_widget.h"
#include <QStyleOptionGraphicsItem>
#include <ui/common/scene_utility.h>

namespace {
    template<class Base>
    class Object: public Base {
    public:
        bool processEvent(QEvent *event) {
            return this->event(event);
        }

        bool staticProcessEvent(QEvent *event) {
            return this->Base::event(event);
        }
    };

    template<class Base>
    Object<Base> *open(Base *object) {
        return static_cast<Object<Base> *>(object);
    }

}


QnMaskedProxyWidget::QnMaskedProxyWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsProxyWidget(parent, windowFlags),
    m_pixmapDirty(true),
    m_updatesEnabled(true)
{}

QnMaskedProxyWidget::~QnMaskedProxyWidget() {
    return;
}

void QnMaskedProxyWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget);

    if (!this->widget() || !this->widget()->isVisible())
        return;

    /* Filter out repaints on the window frame. */
    QRectF renderRectF = option->exposedRect & rect();

    /* Filter out areas that are masked out. */
    if(!m_paintRect.isNull())
        renderRectF = renderRectF & m_paintRect;

    /* Nothing to paint? Just return. */
    QRect renderRect = renderRectF.toAlignedRect();
    if (renderRect.isEmpty())
        return;

    if(m_pixmapDirty && m_updatesEnabled) {
        m_pixmap = QPixmap::grabWidget(this->widget(), this->widget()->rect());
        m_pixmapDirty = false;
    }

    painter->drawPixmap(renderRect, m_pixmap, QRectF(renderRect.topLeft() - rect().topLeft(), renderRect.size()));
}

bool QnMaskedProxyWidget::eventFilter(QObject *object, QEvent *event) {
    if(object == widget()) {
        if(event->type() == QEvent::UpdateRequest) {
            m_pixmapDirty = true;
            /*open(object)->processEvent(event);
            
            qDebug() << QGraphicsItem::d_ptr->needsRepaint;
            return true;*/
        }
    }

    return base_type::eventFilter(object, event);
}

void QnMaskedProxyWidget::setPaintRect(const QRectF &paintRect) {
    if(qFuzzyCompare(m_paintRect, paintRect))
        return;

    m_paintRect = paintRect;
    update();

    emit paintRectChanged();
}

QRectF QnMaskedProxyWidget::paintGeometry() const {
    if(m_paintRect.isNull())
        return QRectF();

    return QRectF(
        pos() + m_paintRect.topLeft(),
        m_paintRect.size()
    );
}

void QnMaskedProxyWidget::setPaintGeometry(const QRectF &paintGeometry) {
    setPaintRect(QRectF(
        paintGeometry.topLeft() - pos(),
        paintGeometry.size()
    ));
}

bool QnMaskedProxyWidget::isUpdatesEnabled() const {
    return m_updatesEnabled;
}

void QnMaskedProxyWidget::setUpdatesEnabled(bool updatesEnabled) {
    m_updatesEnabled = updatesEnabled;

    if(updatesEnabled && m_pixmapDirty)
        update();
}
