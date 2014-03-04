#include "control_background_widget.h"

namespace {
    QLinearGradient borderGradient(Qn::Border border) {
        switch(border) {
            case Qn::TopBorder:     return QLinearGradient(0, 0, 0, 1);
            case Qn::BottomBorder:  return QLinearGradient(0, 1, 0, 0);
            case Qn::LeftBorder:    return QLinearGradient(0, 0, 1, 0);
            case Qn::RightBorder:   return QLinearGradient(1, 0, 0, 0);
            default:                return QLinearGradient();
        }
    }

} // anonymous namespace

QnControlBackgroundWidget::QnControlBackgroundWidget(QGraphicsItem *parent):
    base_type(parent)
{
    init(Qn::TopBorder);
}

QnControlBackgroundWidget::QnControlBackgroundWidget(Qn::Border gradientBorder, QGraphicsItem *parent): 
    base_type(parent) 
{
    init(gradientBorder);
}

void QnControlBackgroundWidget::init(Qn::Border gradientBorder) {
    m_gradientBorder = gradientBorder;
    m_colors << QColor(0, 0, 0, 255) << QColor(0, 0, 0, 64);
    
    setFrameColor(QColor(110, 110, 110, 128));
    setFrameWidth(1.0);

    updateWindowBrush();
}

Qn::Border QnControlBackgroundWidget::gradientBorder() const {
    return m_gradientBorder;
}

void QnControlBackgroundWidget::setGradientBorder(Qn::Border gradientBorder) {
    if(m_gradientBorder == gradientBorder)
        return;

    m_gradientBorder = gradientBorder;

    updateWindowBrush();
}

const QVector<QColor> &QnControlBackgroundWidget::colors() const {
    return m_colors;
}

void QnControlBackgroundWidget::setColors(const QVector<QColor> &colors) {
    if(m_colors == colors)
        return;

    m_colors = colors;

    updateWindowBrush();
}

void QnControlBackgroundWidget::updateWindowBrush() {
    if(m_colors.isEmpty()) {
        setWindowBrush(Qt::transparent);
        return;
    }

    if(m_colors.size() == 1) {
        setWindowBrush(m_colors[0]);
        return;
    }

    QLinearGradient gradient = borderGradient(m_gradientBorder);
    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    gradient.setSpread(QGradient::RepeatSpread);
    for(int i = 0; i < m_colors.size(); i++)
        gradient.setColorAt(0.0 + 1.0 * i / (m_colors.size() - 1), m_colors[i]);
    
    setWindowBrush(gradient);
}