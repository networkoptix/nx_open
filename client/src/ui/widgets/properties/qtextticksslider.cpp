#include "qtextticksslider.h"
#include "utils/common/scoped_painter_rollback.h"
#include "ui/common/color_transformations.h"

QTextTicksSlider::QTextTicksSlider(QWidget* parent): QSlider(parent)
{

}

QTextTicksSlider::QTextTicksSlider(Qt::Orientation orientation, QWidget* parent): QSlider(orientation, parent)
{

}

QTextTicksSlider::~QTextTicksSlider()
{

}

void QTextTicksSlider::paintEvent(QPaintEvent * ev)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    QSlider::paintEvent(ev);
    QPainter painter(this);

    QFontMetrics metrics(painter.font());
    qreal tickWidth = (width()-sliderRect.width()) / qreal(maximum() - minimum());
    qreal xPos = sliderRect.width()/2.0;
    for (int i = minimum(); i <= maximum(); ++i)
    {
        QString text = QString::number(i);
        QSize tickSize = metrics.size(Qt::TextSingleLine, text);
        QPointF p(xPos - tickSize.width()/2.0, height() - tickSize.height() + metrics.descent());
        painter.drawText(p, text);
        xPos += tickWidth;
    }
}
