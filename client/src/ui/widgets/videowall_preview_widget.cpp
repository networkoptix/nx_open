#include "videowall_preview_widget.h"

#include <core/resource/videowall_resource.h>

QnVideowallPreviewWidget::QnVideowallPreviewWidget(QWidget *parent /*= 0*/):
    QWidget(parent),
    m_autoFill(false)
{
    updateModel();
}

QnVideowallPreviewWidget::~QnVideowallPreviewWidget() { }

void QnVideowallPreviewWidget::paintEvent(QPaintEvent *event) {
    QScopedPointer<QPainter> painter(new QPainter(this));

    QRect targetRect = event->rect();
    painter->fillRect(targetRect, palette().window());


}

QnVideoWallResourcePtr QnVideowallPreviewWidget::videowall() const {
    return m_videowall;
}

void QnVideowallPreviewWidget::setVideowall(const QnVideoWallResourcePtr &videowall) {
    if (m_videowall == videowall)
        return;
    m_videowall = videowall;
    updateModel();
}

bool QnVideowallPreviewWidget::autoFill() const {
    return m_autoFill;
}

void QnVideowallPreviewWidget::setAutoFill(bool value) {
    if (m_autoFill == value)
        return;
    m_autoFill = value;
    updateModel();
}

void QnVideowallPreviewWidget::updateModel() {

    if (!m_videowall)
        return;
    update();
}

