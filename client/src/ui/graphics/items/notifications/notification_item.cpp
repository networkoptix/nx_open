#include "notification_item.h"

#include <limits>

#include <QtGui/QApplication>
#include <QtGui/QGraphicsLinearLayout>

#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/image_provider.h>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/notifications/notification_item.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>

namespace {
    const char *actionIndexPropertyName = "_qn_actionIndex";

    const qreal margin = 4.0;
    const qreal colorSignSize = 16.0;
    const qreal closeButtonSize = 12.0;
    const qreal buttonSize = 16.0;

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnNotificationToolTipWidget
// -------------------------------------------------------------------------- //
QnNotificationToolTipWidget::QnNotificationToolTipWidget(QGraphicsItem *parent):
    base_type(parent),
    m_textLabel(new QnProxyLabel(this)),
    m_thumbnailLabel(NULL)
{
    m_textLabel->setAlignment(Qt::AlignLeft);
    m_textLabel->setWordWrap(true);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(m_textLabel);
    setLayout(layout);

    updateTailPos();
}

void QnNotificationToolTipWidget::ensureThumbnail(QnImageProvider* provider) {
    if (m_thumbnailLabel || !provider)
        return;

    QGraphicsLinearLayout *layout = dynamic_cast<QGraphicsLinearLayout*>(this->layout());
    if (!layout)
        return;

    m_thumbnailLabel = new QnClickableProxyLabel(this);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setClickableButtons(Qt::LeftButton);
    setPaletteColor(m_thumbnailLabel, QPalette::Window, Qt::transparent);
    layout->insertItem(0, m_thumbnailLabel);
    connect(m_thumbnailLabel, SIGNAL(clicked()), this, SIGNAL(thumbnailClicked()));

    if (!provider->image().isNull()) {
        m_thumbnailLabel->setPixmap(QPixmap::fromImage(provider->image()));
    } else {
        m_thumbnailLabel->setPixmap(qnSkin->pixmap("events/thumb_loading.png"));
        connect(provider, SIGNAL(imageChanged(const QImage &)), this, SLOT(at_provider_imageChanged(const QImage &)));
        provider->loadAsync();
    }
}

QString QnNotificationToolTipWidget::text() const {
    return m_textLabel->text();
}

void QnNotificationToolTipWidget::setText(const QString &text) {
    m_textLabel->setText(text);
}

void QnNotificationToolTipWidget::updateTailPos()  {
    if (m_enclosingRect.isNull())
        return;

    if (!parentItem())
        return;

    if (!parentItem()->parentItem())
        return;

    QRectF rect = this->rect();
    QGraphicsItem *list = parentItem()->parentItem();

    // half of the tooltip height in coordinates of enclosing rect
    qreal halfHeight = mapRectToItem(list, rect).height() / 2;

    qreal parentPos = parentItem()->mapToItem(list, m_pointTo).y();

    if (parentPos - halfHeight < m_enclosingRect.top())
        setTailPos(QPointF(qRound(rect.right() + 10.0), qRound(rect.top())));
    else
    if (parentPos + halfHeight > m_enclosingRect.bottom())
        setTailPos(QPointF(qRound(rect.right() + 10.0), qRound(rect.bottom())));
    else
        setTailPos(QPointF(qRound(rect.right() + 10.0), qRound((rect.top() + rect.bottom()) / 2)));
    base_type::pointTo(m_pointTo);
}

void QnNotificationToolTipWidget::setEnclosingGeometry(const QRectF &enclosingGeometry) {
    m_enclosingRect = enclosingGeometry;
    updateTailPos();
}

void QnNotificationToolTipWidget::pointTo(const QPointF &pos) {
    m_pointTo = pos;
    base_type::pointTo(pos);
    updateTailPos();
}

void QnNotificationToolTipWidget::at_provider_imageChanged(const QImage &image) {
    if (!m_thumbnailLabel)
        return;
    m_thumbnailLabel->setPixmap(QPixmap::fromImage(image));
}


// -------------------------------------------------------------------------- //
// QnNotificationItem
// -------------------------------------------------------------------------- //
QnNotificationItem::QnNotificationItem(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_defaultActionIdx(-1),
    m_notificationLevel(Qn::OtherNotification),
    m_imageProvider(NULL),
    m_inToolTipPositionUpdate(false)
{
    m_color = notificationColor(m_notificationLevel);

    setClickableButtons(Qt::RightButton | Qt::LeftButton);
    setFrameColor(QColor(110, 110, 110, 255)); // TODO: Same as in workbench_ui. Unify?
    setFrameWidth(0.5);
    setWindowBrush(Qt::transparent);

    m_overlayWidget = new QnSimpleFrameWidget(this);
    m_overlayWidget->setFrameStyle(Qt::NoPen);

    m_textLabel = new QnProxyLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    m_closeButton = new QnImageButtonWidget(this);
    m_closeButton->setCached(true);
    m_closeButton->setToolTip(tr("Close (<b>Right Click</b>)"));
    m_closeButton->setIcon(qnSkin->icon("titlebar/exit.png")); // TODO: #Elric
    m_closeButton->setFixedSize(closeButtonSize, closeButtonSize);
    connect(m_closeButton, SIGNAL(clicked()), this, SIGNAL(closeTriggered()));

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins((colorSignSize - closeButtonSize) / 2.0, 0.0, (colorSignSize - closeButtonSize) / 2.0, 0.0);
    layout->setSpacing(0.0);
    layout->addStretch(1);
    layout->addItem(m_closeButton);
    layout->addStretch(1);

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_layout->setContentsMargins(0.0, margin, margin, margin);
    m_layout->addItem(layout);
    m_layout->addItem(m_textLabel);
    m_layout->setStretchFactor(m_textLabel, 1.0);

    setLayout(m_layout);

    m_tooltipWidget = new QnNotificationToolTipWidget(this);
    m_tooltipWidget->setFocusProxy(this);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);
    m_tooltipWidget->installEventFilter(this);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget, SIGNAL(thumbnailClicked()), this, SLOT(at_thumbnail_clicked()));
    connect(m_tooltipWidget, SIGNAL(tailPosChanged()), this, SLOT(updateToolTipPosition()));
    connect(this, SIGNAL(geometryChanged()), this, SLOT(updateToolTipPosition()));

    m_toolTipHoverProcessor = new HoverFocusProcessor(this);
    m_toolTipHoverProcessor->addTargetItem(this);
    m_toolTipHoverProcessor->addTargetItem(m_tooltipWidget);
    m_toolTipHoverProcessor->setHoverEnterDelay(250);
    m_toolTipHoverProcessor->setHoverLeaveDelay(250);
    connect(m_toolTipHoverProcessor,    SIGNAL(hoverEntered()),  this,  SLOT(updateToolTipVisibility()));
    connect(m_toolTipHoverProcessor,    SIGNAL(hoverLeft()),     this,  SLOT(updateToolTipVisibility()));

    m_hoverProcessor = new HoverFocusProcessor(this);
    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->addTargetItem(m_tooltipWidget);
    connect(m_hoverProcessor,           SIGNAL(hoverEntered()),  this,  SLOT(updateOverlayVisibility()));
    connect(m_hoverProcessor,           SIGNAL(hoverLeft()),     this,  SLOT(updateOverlayVisibility()));

    updateToolTipPosition();
    updateToolTipVisibility();
    updateOverlayGeometry();
    updateOverlayVisibility(false);
    updateOverlayColor();
    updateCloseButtonGeometry();
}

QnNotificationItem::~QnNotificationItem() {
    return;
}

QString QnNotificationItem::text() const {
    return m_textLabel->text();
}

void QnNotificationItem::setText(const QString &text) {
    m_textLabel->setText(text);
}

void QnNotificationItem::setTooltipText(const QString &text) {
    m_tooltipWidget->setText(text);
}

Qn::NotificationLevel QnNotificationItem::notificationLevel() const {
    return m_notificationLevel;
}

QColor QnNotificationItem::notificationColor(Qn::NotificationLevel level) {
    switch (level) {
    case Qn::NoNotification:        return Qt::transparent;
    case Qn::OtherNotification:     return Qt::white;
    case Qn::CommonNotification:    return qnGlobals->notificationColorCommon();
    case Qn::ImportantNotification: return qnGlobals->notificationColorImportant();
    case Qn::CriticalNotification:  return qnGlobals->notificationColorCritical();
    case Qn::SystemNotification:    return qnGlobals->notificationColorSystem();
    default:
        qnWarning("Invalid notification level '%1'.", static_cast<int>(level));
        return QColor();
    }
}

void QnNotificationItem::setNotificationLevel(Qn::NotificationLevel notificationLevel) {
    if(m_notificationLevel == notificationLevel)
        return;

    m_notificationLevel = notificationLevel;
    m_color = notificationColor(m_notificationLevel);

    updateOverlayColor();

    emit notificationLevelChanged();
}

void QnNotificationItem::setImageProvider(QnImageProvider* provider) {
    m_imageProvider = provider;
}

void QnNotificationItem::setTooltipEnclosingRect(const QRectF &rect) {
    m_tooltipWidget->setEnclosingGeometry(rect);
}

void QnNotificationItem::setGeometry(const QRectF &geometry) {
    QSizeF oldSize = size();

    base_type::setGeometry(geometry);

    if(!qFuzzyCompare(size(), oldSize)) {
        updateOverlayGeometry();
        updateCloseButtonGeometry();
    }
}

void QnNotificationItem::addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
                                         const QnActionParameters &parameters, bool defaultAction)
{
    qreal buttonSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

    QnImageButtonWidget *button = new QnImageButtonWidget(this);

    button->setIcon(icon);
    button->setToolTip(tooltip);
    button->setCached(true);
    button->setFixedSize(buttonSize);
    button->setProperty(actionIndexPropertyName, m_actions.size());
    if (m_defaultActionIdx < 0 || defaultAction)
        m_defaultActionIdx = m_actions.size();

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->setSpacing(0.0);
    layout->addStretch(1);
    layout->addItem(button);
    layout->addStretch(1);
    m_layout->addItem(layout);

    connect(button, SIGNAL(clicked()), this, SLOT(at_button_clicked()));
    m_actions << ActionData(actionId, parameters);

    m_textLabel->setToolTip(tooltip); // TODO: #Elric
}

void QnNotificationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    painter->setPen(QPen(QColor(110, 110, 110, 255), 0.5));
    painter->setBrush(QBrush(toTransparent(m_color, 0.5)));

    QRectF rect = this->rect();
    QPointF d = QPointF(colorSignSize, colorSignSize) / 2.0;
    QPointF center = QPointF(rect.left() + colorSignSize / 2.0, (rect.top() + rect.bottom()) / 2.0);

    painter->drawRect(QRectF(center - d, center + d));
}

void QnNotificationItem::hideToolTip() {
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(0.0);
}

void QnNotificationItem::showToolTip() {
    m_tooltipWidget->ensureThumbnail(m_imageProvider);
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(1.0);
}

void QnNotificationItem::updateToolTipVisibility() {
    if(m_toolTipHoverProcessor->isHovered() && !m_tooltipWidget->text().isEmpty()) {
        showToolTip();
    } else {
        hideToolTip();
    }
}

void QnNotificationItem::updateToolTipPosition() {
    if(m_inToolTipPositionUpdate)
        return;

    QnScopedValueRollback<bool> guard(&m_inToolTipPositionUpdate, true); /* Prevent stack overflow. */

    m_tooltipWidget->updateTailPos();
    m_tooltipWidget->pointTo(QPointF(0, qRound(geometry().height() / 2 )));
}

void QnNotificationItem::updateOverlayGeometry() {
    m_overlayWidget->setGeometry(QRectF(QPointF(0, 0), size()));
}

void QnNotificationItem::updateCloseButtonGeometry() {
    //m_closeButton->setGeometry(QRectF(rect().topRight() - QPointF(m_closeButton->size().width(), 0.0), m_closeButton->size()));
}

void QnNotificationItem::updateOverlayVisibility(bool animate) {
    if(m_actions.isEmpty()) {
        m_overlayWidget->setOpacity(0);
        m_closeButton->setOpacity(0);
        return; // TODO: #Elric?
    }

    qreal opacity = m_hoverProcessor->isHovered() ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_overlayWidget, 4.0)->animateTo(opacity);
        opacityAnimator(m_closeButton, 4.0)->animateTo(opacity);
    } else {
        m_overlayWidget->setOpacity(opacity);
        m_closeButton->setOpacity(opacity);
    }
}

void QnNotificationItem::updateOverlayColor() {
    m_overlayWidget->setWindowBrush(toTransparent(m_color, 0.3));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnNotificationItem::clicked(Qt::MouseButton button) {
    if(button == Qt::RightButton) {
        emit closeTriggered();
    } else if(button == Qt::LeftButton) {
        if(!m_actions.isEmpty()) {
            ActionData data = m_actions[0]; // TODO: #Elric
            emit actionTriggered(data.action, data.params);
        }
    }
}

void QnNotificationItem::at_button_clicked() {
    int idx = sender()->property(actionIndexPropertyName).toInt();
    if (m_actions.size() <= idx)
        return;
    ActionData data = m_actions[idx];

    emit actionTriggered(data.action, data.params);
}

void QnNotificationItem::at_thumbnail_clicked() {
    if (m_defaultActionIdx < 0)
        return;
    if (m_actions.size() <= m_defaultActionIdx)
        return;
    ActionData data = m_actions[m_defaultActionIdx];

    emit actionTriggered(data.action, data.params);
}