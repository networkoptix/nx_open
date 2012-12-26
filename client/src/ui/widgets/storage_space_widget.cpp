#include "storage_space_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

QnStorageSpaceWidget::QnStorageSpaceWidget(QWidget *parent) :
    base_type(parent)
{

}

QnStorageSpaceWidget::~QnStorageSpaceWidget()
{

}

void QnStorageSpaceWidget::setValue(int value) {
    base_type::setValue(qMax(m_lockedValue, value));
}


void QnStorageSpaceWidget::paintEvent(QPaintEvent *ev)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);


    QPainter p(this);
    /*QPainterPath path;

    int d = grooveRect.height();
    qreal r = d *0.15;
    path.addRoundedRect(grooveRect.adjusted(d, 0, -d, 0), r, r);
    p.fillPath(path, QBrush(QColor(Qt::red)));*/

    qreal w = grooveRect.width();
    qreal top = grooveRect.top();
    qreal height = grooveRect.height();
    int size = maximum();

    qreal lockedWidth = w * m_lockedValue / size;
    QRectF locked(grooveRect.left(), top, lockedWidth, height);

    qreal recordedWidth = w * (m_recordedValue - m_lockedValue) / size;
    QRectF recorded(locked.right(), top, recordedWidth, height);

    qreal reservedWidth = w * (value() - m_recordedValue) / size;
    QRectF reserved(recorded.right(), top, reservedWidth, height);

    p.fillRect(grooveRect, QBrush(QColor(Qt::white)));
    p.fillRect(locked, QBrush(QColor(Qt::gray)));
    p.fillRect(recorded, QBrush(QColor(Qt::red)));
    p.fillRect(reserved, QBrush(QColor(Qt::green)));

    base_type::paintEvent(ev);
}
