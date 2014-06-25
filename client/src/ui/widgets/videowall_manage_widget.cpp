#include "videowall_manage_widget.h"

#include <ui/widgets/videowall_manage_widget_p.h>

#include <QtGui/QIcon>

#include <client/client_settings.h>

#include <core/resource/videowall_resource.h>

#include <ui/style/skin.h>

#include <utils/common/string.h>
#include <utils/common/scoped_painter_rollback.h>

QnVideowallManageWidget::QnVideowallManageWidget(QWidget *parent /*= 0*/):
    base_type(parent),
    d_ptr(new QnVideowallManageWidgetPrivate())
{
    installEventFilter(this);
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
    return base_type::eventFilter(target, event);
}

void QnVideowallManageWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(QnVideowallManageWidget);
    /* if (&& !d->skipNextReleaseEvent) */
    {
        QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
        QPoint p = transform.map(event->pos());
        QUuid id = d->itemIdByPos(p);
        if (id.isNull())
            return;

        d->addItemTo(id);
        update();
    }
    //d->skipNextReleaseEvent = false;
}

void QnVideowallManageWidget::mouseMoveEvent(QMouseEvent *event) {
    Q_D(QnVideowallManageWidget);

    QTransform transform(d->getInvertedTransform(d->targetRect(this->rect())));
    qDebug() << transform.map(event->pos());
}

