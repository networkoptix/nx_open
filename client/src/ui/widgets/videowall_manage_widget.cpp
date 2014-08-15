#include "videowall_manage_widget.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>
#include <ui/processors/drag_processor.h>
#include <ui/widgets/videowall_manage_widget_p.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {
    const QSize minimumWidgetSizeHint(400, 300);
}

QnVideowallManageWidget::QnVideowallManageWidget(QWidget *parent /*= 0*/):
    base_type(parent),
    d_ptr(new QnVideowallManageWidgetPrivate(this)),
    m_dragProcessor(new DragProcessor(this)),
    m_skipReleaseEvent(false),
    m_pressedButtons(Qt::NoButton)
{
    installEventFilter(this);
    setMouseTracking(true);

    m_dragProcessor->setHandler(this);

    QAnimationTimer *animationTimer = new QAnimationTimer(this);
    setTimer(animationTimer);
    startListening();
    
    connect(d_ptr.data(), &QnVideowallManageWidgetPrivate::itemsChanged, this, &QnVideowallManageWidget::itemsChanged);
}

QnVideowallManageWidget::~QnVideowallManageWidget() { }

void QnVideowallManageWidget::paintEvent(QPaintEvent *event) {
    Q_D(QnVideowallManageWidget);

    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect paintRect = this->rect();
    if (paintRect.isNull())
        return;

    painter->fillRect(paintRect, palette().window());
    d->paint(painter.data(), paintRect);
}

void QnVideowallManageWidget::loadFromResource(const QnVideoWallResourcePtr &videowall) {
    Q_D(QnVideowallManageWidget);
    d->loadFromResource(videowall);
    update();
}

void QnVideowallManageWidget::submitToResource(const QnVideoWallResourcePtr &videowall) {
    Q_D(QnVideowallManageWidget);
    d->submitToResource(videowall);
}

bool QnVideowallManageWidget::eventFilter(QObject *target, QEvent *event) {
    if (event->type() == QEvent::LeaveWhatsThisMode)
        m_skipReleaseEvent = true;
    else if (event->type() == QEvent::Enter) {
        Q_D(QnVideowallManageWidget);
        d->mouseEnter();
    } else if (event->type() == QEvent::Leave) {
        Q_D(QnVideowallManageWidget);
        d->mouseLeave();
    }
    m_dragProcessor->widgetEvent(this, event);
    return base_type::eventFilter(target, event);
}

void QnVideowallManageWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);
    m_pressedButtons = event->buttons();
}

void QnVideowallManageWidget::mouseMoveEvent(QMouseEvent *event) {
    base_type::mouseMoveEvent(event);

    if (m_dragProcessor->isRunning())
        return;

    Q_D(QnVideowallManageWidget);

    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    d->mouseMoveAt(transform.map(event->pos()));  
}

void QnVideowallManageWidget::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    bool skip = m_skipReleaseEvent;
    m_skipReleaseEvent = false;
    if (skip)
        return;

    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(event->pos());
    d->mouseClickAt(p, m_pressedButtons);
}


void QnVideowallManageWidget::startDrag(DragInfo *info) {
    m_skipReleaseEvent = true;

    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));

    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    d->dragStartAt(p);
}

void QnVideowallManageWidget::dragMove(DragInfo *info) {
    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    QPoint prev = transform.map(mapFromGlobal(info->lastMouseScreenPos()));
    d->dragMoveAt(p, prev);
}

void QnVideowallManageWidget::finishDrag(DragInfo *info) {
    Q_D(QnVideowallManageWidget);
    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    QPoint p = transform.map(mapFromGlobal(info->mouseScreenPos()));
    d->dragEndAt(p);
}

void QnVideowallManageWidget::tick(int deltaTime) {
    Q_D(QnVideowallManageWidget);
    d->tick(deltaTime);
    update();
}

const QnVideowallManageWidgetColors & QnVideowallManageWidget::colors() const {
    return m_colors;
}

void QnVideowallManageWidget::setColors(const QnVideowallManageWidgetColors &colors) {
    m_colors = colors;
    update();
}

int QnVideowallManageWidget::proposedItemsCount() const {
    Q_D(const QnVideowallManageWidget);
    return d->proposedItemsCount();
}

QSize QnVideowallManageWidget::minimumSizeHint() const {
    Q_D(const QnVideowallManageWidget);
    QRect source(QPoint(0, 0), minimumWidgetSizeHint);
    return d->targetRect(source).size();  
}
