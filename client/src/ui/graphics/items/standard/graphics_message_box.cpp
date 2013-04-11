#include "graphics_message_box.h"

#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QPainter>


QnGraphicsMessageBoxItem *instance = NULL;

QnGraphicsMessageBoxItem::QnGraphicsMessageBoxItem(QGraphicsItem *parent):
    base_type(parent)
{
    instance = this;
    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(2.0);
    setLayout(m_layout);

}

QnGraphicsMessageBoxItem::~QnGraphicsMessageBoxItem() {
    instance = NULL;
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

QnGraphicsMessageBox::QnGraphicsMessageBox(QGraphicsItem *parent) :
    base_type(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
}

QnGraphicsMessageBox::~QnGraphicsMessageBox() {
}

void QnGraphicsMessageBox::information(const QString &text) {

    if (!instance)
        return;

    QnGraphicsMessageBox* box = new QnGraphicsMessageBox(instance);
    box->setText(text);
    box->setFrameShape(GraphicsFrame::StyledPanel);
    box->setLineWidth(5);
    box->setFrameRect(QRectF(200, 200, 400, 200));

    QFont font;
    font.setPixelSize(22);
    box->setFont(font);
    instance->addItem(box);

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
