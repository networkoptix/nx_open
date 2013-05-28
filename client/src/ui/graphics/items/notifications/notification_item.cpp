#include "notification_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/notifications/notification_item.h>

#include <ui/style/skin.h>

QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_textLabel(new GraphicsLabel(this)),
    m_image(new QnImageButtonWidget(this)),
    m_layout(new QGraphicsLinearLayout(Qt::Horizontal)),
    m_color(Qt::red)
{
    m_image->setMinimumSize(QSizeF(24, 24));
    m_image->setMaximumSize(QSizeF(24, 24));

    setIconPath(QLatin1String("item/zoom_window.png"));
    setText(tr("Create Zoom Window"));

    m_layout->setContentsMargins(2.5, 2.5, 2.5, 2.5);
    m_layout->addItem(m_textLabel);
    m_layout->addStretch();
    m_layout->addItem(m_image);

    setLayout(m_layout);
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
    painter->fillRect(rect(), m_color);
}
