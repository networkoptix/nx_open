
#include "separator.h"

QnSeparator::QnSeparator(QGraphicsItem *parent)
    : QnFramedWidget(parent)
    , m_lastSize()
{
    init();
}

QnSeparator::QnSeparator(const QColor &color
    , QGraphicsItem *parent)
    : QnFramedWidget(parent)
    , m_lastSize()
{
    init();
    setLineColor(color);
}

void QnSeparator::init()
{
    enum { kDefaultLineWidth = 1 };

    setFrameShape(Qn::CustomFrame);
    setLineWidth(kDefaultLineWidth, true);
    updateLinePos();

    connect(this, &QnFramedWidget::geometryChanged
        , this, [this]() { updateLinePos(); });
}


QnSeparator::~QnSeparator()
{
}

qreal QnSeparator::lineWidth() const
{
    return frameWidth();
}

void QnSeparator::setLineWidth(qreal lineWidth
    , bool changeHeight)
{
    setFrameWidth(lineWidth);

    if (changeHeight)
        setMaximumHeight(lineWidth * 2);
}

QColor QnSeparator::lineColor() const
{
    return frameColor();
}

void QnSeparator::setLineColor(const QColor &color)
{
    setFrameColor(color);
}

void QnSeparator::updateLinePos()
{
    const auto newSize = rect().size();
    if (newSize == m_lastSize)
        return;

    m_lastSize = newSize;

    const auto y = newSize.height() / 2;

    QPainterPath path;
    path.moveTo(0, y);
    path.lineTo(newSize.width(), y);

    setCustomFramePath(path);
}

