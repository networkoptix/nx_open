#include "notification_item.h"

#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/notifications/notification_item.h>

namespace {
    const qreal margin = 4;
    const qreal totalWidth = 200;
    const qreal totalHeight = 24;
    const qreal spacerSize = totalHeight * .5;
}

QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_textLabel(new QnProxyLabel(this)),
    m_image(new QnImageButtonWidget(this)),
    m_color(Qt::red)
{
    m_textLabel->setWordWrap(true);
    {
        QPalette palette = m_textLabel->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        m_textLabel->setPalette(palette);
    }

    m_image->setMinimumSize(QSizeF(totalHeight, totalHeight));
    m_image->setMaximumSize(QSizeF(totalHeight, totalHeight));
    m_image->setVisible(false);

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(margin*2, margin, margin, margin);
    layout->addItem(m_textLabel);
//    layout->addStretch();
    layout->addItem(m_image);
    layout->setStretchFactor(m_textLabel, 1.0);

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

void QnNotificationItem::setIcon(const QIcon &icon) {
    m_image->setIcon(icon);
    m_image->setVisible(true);
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
    painter->fillRect(boundingRect(), QBrush(gradient));

}
