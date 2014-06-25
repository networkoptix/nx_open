#include "videowall_manage_widget.h"

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>
#include <ui/processors/drag_processor.h>
#include <ui/widgets/videowall_manage_widget_p.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

QnVideowallManageWidget::QnVideowallManageWidget(QWidget *parent /*= 0*/):
    base_type(parent),
    d_ptr(new QnVideowallManageWidgetPrivate())
{
    installEventFilter(this);
    setMouseTracking(true);

    m_dragProcessor = new DragProcessor(this);
    m_dragProcessor->setHandler(this);
}

QnVideowallManageWidget::~QnVideowallManageWidget() { }

void QnVideowallManageWidget::paintEvent(QPaintEvent *event) {
    Q_D(QnVideowallManageWidget);

    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect eventRect = event->rect();
    if (eventRect.isNull())
        return;

    painter->fillRect(eventRect, palette().window());
    d->paint(painter.data(), eventRect);
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
//     if (event->type() == QEvent::LeaveWhatsThisMode)
//         d->skipNextReleaseEvent = true;
    m_dragProcessor->widgetEvent(this, event);
    return base_type::eventFilter(target, event);
}

void QnVideowallManageWidget::mouseReleaseEvent(QMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    Q_D(QnVideowallManageWidget);
    /* if (&& !d->skipNextReleaseEvent) */
    {
        QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
        QPoint p = transform.map(event->pos());
        //d->mouseReleaseAt(p);
        QUuid id = d->itemIdByPos(p);
        if (id.isNull())
            return;

        d->addItemTo(id);
        update();
    }
    //d->skipNextReleaseEvent = false;
}

void QnVideowallManageWidget::mouseMoveEvent(QMouseEvent *event) {
    base_type::mouseMoveEvent(event);

    Q_D(QnVideowallManageWidget);

    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    qDebug() << transform.map(event->pos()) << event->buttons();

    
}

void QnVideowallManageWidget::mousePressEvent(QMouseEvent *event) {
    base_type::mousePressEvent(event);


}

void QnVideowallManageWidget::startDragProcess(DragInfo *info) {
    qDebug() << "start drag process";
}

void QnVideowallManageWidget::dragMove(DragInfo *info) {
    qDebug() << "drag move";
}

void QnVideowallManageWidget::finishDragProcess(DragInfo *info) {
    qDebug() << "finish drag process";
}

