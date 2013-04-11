#include "graphics_message_box.h"

#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <ui/graphics/items/standard/graphics_frame.h>
#include <utils/common/scoped_painter_rollback.h>

QnGraphicsMessageBoxItem *instance = NULL;

QnGraphicsMessageBoxItem::QnGraphicsMessageBoxItem(QGraphicsItem *parent):
    base_type(parent)
{
    instance = this;
    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(2.0);
    setLayout(m_layout);
    setAcceptedMouseButtons(Qt::NoButton);

    setOpacity(0.7);
}

QnGraphicsMessageBoxItem::~QnGraphicsMessageBoxItem() {
  //  instance = NULL;
}

void QnGraphicsMessageBoxItem::addItem(QGraphicsLayoutItem *item) {
    m_layout->addItem(item);
}

QRectF QnGraphicsMessageBoxItem::boundingRect() const {
    return base_type::boundingRect();
}

void QnGraphicsMessageBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);
}

//-----------------------QnGraphicsMessageBox---------------------------------------//


QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent, const QString &text) :
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setText(text);
    setFrameShape(GraphicsFrame::NoFrame);

    QFont font;
    font.setPixelSize(22);
    setFont(font);

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, QColor(166, 166, 166));
    setPalette(palette);

    setAcceptedMouseButtons(Qt::NoButton);
}

QnGraphicsMessageBox::~QnGraphicsMessageBox() {
}

void QnGraphicsMessageBox::information(const QString &text) {

    if (!instance)
        return;

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(instance, text);
    instance->addItem(box);
}

int QnGraphicsMessageBox::getTimeout() {
    return 3;
}

QSizeF QnGraphicsMessageBox::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    QSizeF hint = base_type::sizeHint(which, constraint);
    return hint + QSizeF(18*2, 18*2);
}

void QnGraphicsMessageBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
/*
    m_overlayLabel->setStyleSheet(QLatin1String("QLabel { font-size:22px; border-width: 2px;"\
                                                "border-style: inset; border-color: #535353;"\
                                                "border-radius: 18px;"\
                                                "background: #212150; color: #a6a6a6; selection-background-color: lightblue }"));

*/
    QPainterPath path;
    path.addRoundedRect(rect(), 18, 18);

    QnScopedPainterPenRollback penRollback(painter, QColor(83, 83, 83));
    QnScopedPainterBrushRollback brushRollback(painter, QColor(33, 33, 80));
    painter->drawPath(path);
    Q_UNUSED(penRollback)
    Q_UNUSED(brushRollback)

    base_type::paint(painter, option, widget);
}

