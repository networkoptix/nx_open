#include "framed_label.h"

namespace {
    const int labelFrameWidth = 4;  // in pixels
}

QnFramedLabel::QnFramedLabel(QWidget* parent):
    base_type(parent),
    m_opacityPercent(100)
{}

QnFramedLabel::~QnFramedLabel() {}

QSize QnFramedLabel::size() const {
    return base_type::size() - QSize(labelFrameWidth*2, labelFrameWidth*2);
}

int QnFramedLabel::opacityPercent() const {
    return m_opacityPercent;
}

void QnFramedLabel::setOpacityPercent(int value) {
    if (m_opacityPercent == value)
        return;
    m_opacityPercent = value;
    repaint();
}

void QnFramedLabel::debugSize() {
    qDebug() << "label resize event received" << size();
}

void QnFramedLabel::resizeEvent(QResizeEvent *event) {
    debugSize();
}

void QnFramedLabel::paintEvent(QPaintEvent *event) {
    bool pixmapExists = pixmap() && !pixmap()->isNull();
    if (!pixmapExists)
        base_type::paintEvent(event);

    QPainter painter(this);
    QRect fullRect = event->rect().adjusted(labelFrameWidth / 2, labelFrameWidth / 2, -labelFrameWidth / 2, -labelFrameWidth / 2);

    if (pixmapExists) {
        painter.setOpacity(0.01 * m_opacityPercent);
        QRect pix = pixmap()->rect();
        int x = fullRect.left() + (fullRect.width() - pix.width()) / 2;
        int y = fullRect.top() + (fullRect.height() - pix.height()) / 2;
        painter.drawPixmap(x, y, *pixmap());
    }

    painter.setOpacity(0.5);
    QPen pen;
    pen.setWidth(labelFrameWidth);
    pen.setColor(QColor(Qt::lightGray));
    painter.setPen(pen);
    painter.drawRect(fullRect);
}
