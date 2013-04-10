#include "graphics_message_box.h"

#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QPainter>

QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent) :
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
}

QnGraphicsMessageBox::~QnGraphicsMessageBox() {
}

void QnGraphicsMessageBox::information(QGraphicsItem* item, const QString &text) {

//    QWidget *viewport = view->childAt(0, 0);
//    QGraphicsItem* item = view->itemAt(0, 0);

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(item);
    box->setText(text);
    box->setFrameShape(GraphicsFrame::StyledPanel);
    box->setLineWidth(5);
    box->setFrameRect(QRectF(200, 200, 400, 200));

    QFont font;
    font.setPixelSize(22);
    box->setFont(font);

    item->scene()->addItem(box);
    box->setZValue(std::numeric_limits<qreal>::max());

//    m_messages.append(msg);

    //QWidget *viewport = view->childAt(pos);


}

int QnGraphicsMessageBox::getTimeout() {
    return 3;
}


void QnGraphicsMessageBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QStyleOptionFrame opt;
    opt.rect = rect().toRect();

    style()->drawPrimitive(QStyle::PE_PanelTipLabel, &opt, painter);
    base_type::paint(painter, option, widget);
}
