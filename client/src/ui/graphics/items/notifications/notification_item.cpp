#include "notification_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/notifications/notification_item.h>

#include <ui/style/skin.h>

QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    m_image = new QnImageButtonWidget(this);
    m_image->setIcon(qnSkin->icon("item/zoom_window.png"));
    m_image->setToolTip(tr("Create Zoom Window"));

    m_textLabel = new GraphicsLabel(this);
    m_textLabel->setText(tr("Ololo - testing notifications"));

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_layout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    m_layout->addItem(m_textLabel);
    m_layout->addStretch();
    m_layout->addItem(m_image);

    setLayout(m_layout);
}

QnNotificationItem::~QnNotificationItem() {
}
