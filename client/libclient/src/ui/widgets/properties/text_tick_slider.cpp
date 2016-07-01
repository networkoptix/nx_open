
#include "text_tick_slider.h"

#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionSlider>


QnTextTickSlider::QnTextTickSlider(QWidget *parent): 
    QSlider(parent)
{}

QnTextTickSlider::QnTextTickSlider(Qt::Orientation orientation, QWidget *parent): 
    QSlider(orientation, parent)
{}

QnTextTickSlider::~QnTextTickSlider() {
    return;
}

QSize QnTextTickSlider::minimumSizeHint() const {
    QSize result = base_type::minimumSizeHint();
    if(result.isEmpty())
        result = QSize(0, 0); /* So that we don't end up with an invalid size on the next step. */

    QFontMetrics metrics(font());
    result.setHeight(result.height() + metrics.ascent());

    return result;
}

void QnTextTickSlider::changeEvent(QEvent *event) {
    if(event->type() == QEvent::FontChange)
        updateGeometry(); 

    base_type::changeEvent(event);
}

void QnTextTickSlider::paintEvent(QPaintEvent *event) {
    base_type::paintEvent(event);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    QPainter painter(this);

    QFontMetrics metrics(painter.font());
    qreal tickWidth = (width() - sliderRect.width()) / qreal(maximum() - minimum());
    qreal xPos = sliderRect.width() / 2.0;
    for (int i = minimum(); i <= maximum(); ++i) {
        QString text = QString::number(i);
        QSize tickSize = metrics.size(Qt::TextSingleLine, text);
        
        painter.setPen(Qt::darkGray);
        if (i != value())
            painter.drawLine(xPos, height() / 2 - 2, xPos, height() / 2 + 2);

        painter.setPen(Qt::white);
        QPointF p(xPos - tickSize.width() / 2.0, (height() - tickSize.height()) + metrics.ascent());
        painter.drawText(p, text);
        xPos += tickWidth;
    }
}
