#include "notification_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/notifications/notification_item.h>

#include <ui/style/skin.h>

namespace {
    const qreal margin = 4;
    const qreal totalWidth = 200;
    const qreal totalHeight = 24;
    const qreal spacerSize = totalHeight * .5;
}

QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_textLabel(new GraphicsLabel(this)),
    m_image(new QnImageButtonWidget(this)),
    m_color(Qt::red)
{
//    QSizeF size(totalWidth + margin*2, totalHeight + margin*2);
//    setMinimumSize(size);
//    setMaximumSize(size);

    m_image->setMinimumSize(QSizeF(totalHeight, totalHeight));
    m_image->setMaximumSize(QSizeF(totalHeight, totalHeight));
//    m_image->setPos(totalWidth - totalHeight + margin, margin);

 //   m_textLabel->setMinimumSize(totalWidth - totalHeight - spacerSize, totalHeight);
 //   m_textLabel->setMaximumSize(totalWidth - totalHeight - spacerSize, totalHeight);
//    m_textLabel->setPos(margin + spacerSize, margin);

    setIconPath(QLatin1String("item/zoom_window.png"));
    setText(tr("Create Zoom Window"));

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(margin*2, margin, margin, margin);
    layout->addItem(m_textLabel);
    layout->addStretch();
    layout->addItem(m_image);

    setLayout(layout);


}

QnNotificationItem::~QnNotificationItem() {
}

QString QnNotificationItem::text() const {
    return m_textLabel->text();
}

void QnNotificationItem::setText(const QString &text) {
    m_textLabel->setText(text);
    m_image->setToolTip(text);
}

QString QnNotificationItem::iconPath() const {
    return m_iconPath;
}

void QnNotificationItem::setIconPath(const QString& iconPath) {
    if (m_iconPath == iconPath)
        return;
    m_iconPath = iconPath;
    m_image->setIcon(qnSkin->icon(m_iconPath));
}

QColor QnNotificationItem::color() const {
    return m_color;
}

void QnNotificationItem::setColor(const QColor &color) {
    m_color = color;
}

void QnNotificationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QRadialGradient gradient(margin, totalHeight*.5 + margin, totalHeight*2);
    gradient.setColorAt(0.0, m_color);
    QColor gradientTo(m_color);
    gradientTo.setAlpha(64);
    gradient.setColorAt(0.5, gradientTo);
    gradient.setColorAt(1.0, Qt::transparent);

    gradient.setSpread(QGradient::PadSpread);
//    painter->fillRect(QRectF(0, 0, totalHeight*2, totalHeight + margin*2), QBrush(gradient));
    painter->fillRect(boundingRect(), QBrush(gradient));

}
