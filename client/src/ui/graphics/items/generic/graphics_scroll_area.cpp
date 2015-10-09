#include "graphics_scroll_area.h"

class QnGraphicsScrollAreaPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnGraphicsScrollArea)
    QnGraphicsScrollArea *q_ptr;
public:
    QnGraphicsScrollAreaPrivate(QnGraphicsScrollArea *parent);

    QGraphicsWidget *contentWidget;
    int contentY;
    qreal contentHeight;
    Qt::Alignment alignment;
    int lineHeight;

    void at_contentWidgetHeightChanged();
    void fitToBounds();
};

QnGraphicsScrollArea::QnGraphicsScrollArea(QGraphicsItem *parent)
    : base_type(parent)
    , d_ptr(new QnGraphicsScrollAreaPrivate(this))
{
    Q_D(QnGraphicsScrollArea);
    setAcceptedMouseButtons(0);
    connect(this, &QnGraphicsScrollArea::heightChanged, d, &QnGraphicsScrollAreaPrivate::fitToBounds);
}

QnGraphicsScrollArea::~QnGraphicsScrollArea() {
}

QGraphicsWidget *QnGraphicsScrollArea::contentWidget() const {
    Q_D(const QnGraphicsScrollArea);
    return d->contentWidget;
}

void QnGraphicsScrollArea::setContentWidget(QGraphicsWidget *widget) {
    Q_D(QnGraphicsScrollArea);

    if (d->contentWidget == widget)
        return;

    if (d->contentWidget) {
        disconnect(d->contentWidget, nullptr, this, nullptr);
        d->contentWidget->setParentItem(nullptr);
    }

    d->contentWidget = widget;
    d->contentWidget->setParentItem(this);

    connect(d->contentWidget, &QGraphicsWidget::heightChanged, d, &QnGraphicsScrollAreaPrivate::at_contentWidgetHeightChanged);
    d->at_contentWidgetHeightChanged();
}

Qt::Alignment QnGraphicsScrollArea::alignment() const {
    Q_D(const QnGraphicsScrollArea);

    return d->alignment;
}

void QnGraphicsScrollArea::setAlignment(Qt::Alignment alignment) {
    Q_D(QnGraphicsScrollArea);

    if (d->alignment == alignment)
        return;

    d->alignment = alignment;

    d->fitToBounds();
}

void QnGraphicsScrollArea::wheelEvent(QGraphicsSceneWheelEvent *event) {
    Q_D(QnGraphicsScrollArea);

    if (event->orientation() == Qt::Vertical) {
        if (d->contentHeight <= size().height()) {
            event->ignore();
            return;
        }

        int dy = -event->delta() / 120 * d->lineHeight * qApp->wheelScrollLines();
        d->contentY += dy;
        d->fitToBounds();
    }

    event->accept();
}


QnGraphicsScrollAreaPrivate::QnGraphicsScrollAreaPrivate(QnGraphicsScrollArea *parent)
    : QObject(parent)
    , q_ptr(parent)
    , contentWidget(nullptr)
    , contentY(0)
    , lineHeight(0)
{
    Q_Q(QnGraphicsScrollArea);
    QFontMetrics fm(q->font());
    lineHeight = fm.height();
}

void QnGraphicsScrollAreaPrivate::at_contentWidgetHeightChanged() {
    if (!contentWidget)
        return;

    if (contentHeight == contentWidget->size().height())
        return;

    contentHeight = contentWidget->size().height();

    fitToBounds();
}

void QnGraphicsScrollAreaPrivate::fitToBounds() {
    if (!contentWidget)
        return;

    Q_Q(QnGraphicsScrollArea);

    auto height = q->size().height();

    if (contentHeight <= height) {
        if (alignment.testFlag(Qt::AlignBottom))
            contentY = contentHeight - height;
        else // do align top
            contentY = 0;
    } else {
        if (contentHeight - contentY < height)
            contentY = contentHeight - height;

        if (contentY < 0)
            contentY = 0;
    }

    contentWidget->setPos(0, -contentY);
}
