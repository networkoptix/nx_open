#include "centralwidget.h"

#include <QtCore/QPropertyAnimation>
#include <QtCore/QVector>

#include <QtGui/QPainter>

#include "celllayout.h"

CentralWidget::CentralWidget(QGraphicsItem *parent)
    : QGraphicsWidget(parent),
      m_gridOpacity(0.0)
{
    m_gridOpacityAnimation = new QPropertyAnimation(this, "gridOpacity", this);
    m_gridOpacityAnimation->setDuration(1000);
    m_gridOpacityAnimation->setEasingCurve(QEasingCurve::InOutSine);
}

CentralWidget::~CentralWidget()
{
}

float CentralWidget::gridOpacity() const
{
    return m_gridOpacity;
}

void CentralWidget::setGridOpacity(float opacity)
{
    m_gridOpacity = opacity;
    update();
}

void CentralWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    CellLayout *gridLayout = static_cast<CellLayout *>(layout());
    if (!gridLayout)
        return;

    const QSizeF cellSize = gridLayout->cellSize();
    const float spacing = gridLayout->verticalSpacing();

    const QRectF r = gridLayout->contentsRect().adjusted(-spacing / 2, -spacing / 2, spacing / 2, spacing / 2);

    QVector<QLineF> lines;
    // vertical lines
    for (int column = 0; column < gridLayout->columnCount() + 1; ++column)
        lines.append(QLineF(QPointF(r.left() + (cellSize.width() + spacing) * column, r.top()), QPointF(r.left() + (cellSize.width() + spacing) * column, r.bottom())));
    // horizontal lines
    for (int row = 0; row < gridLayout->rowCount() + 1; ++row)
        lines.append(QLineF(QPointF(r.left(), r.top() + (cellSize.height() + spacing) * row), QPointF(r.right(), r.top() +  + (cellSize.height() + spacing) * row)));

    QPen oldPen = painter->pen();
    painter->setPen(QPen(QColor(0, 240, 240), 1, Qt::SolidLine));

    painter->drawLines(lines);

    painter->setPen(oldPen);
}
