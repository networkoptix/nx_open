#include "notification_widget.h"

#include <limits>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/common/geometry.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/media/audio_player.h>
#include <utils/image_provider.h>

namespace {
    const char *actionIndexPropertyName = "_qn_actionIndex";

    const qreal margin = 4.0;
    const QSize colorSignSize(8.0, 16.0);
    const qreal closeButtonSize = 12.0;
    const qreal buttonSize = 16.0;

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnNotificationToolTipWidget
// -------------------------------------------------------------------------- //
QnNotificationToolTipWidget::QnNotificationToolTipWidget(QGraphicsItem *parent):
    base_type(parent),
    m_thumbnailLabel(NULL)
{
    setClickableButtons(Qt::RightButton);

    m_textLabel = new QnProxyLabel(this);
    m_textLabel->setAlignment(Qt::AlignLeft);
    m_textLabel->setWordWrap(true);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    QnImageButtonWidget *closeButton = new QnImageButtonWidget(this);
    closeButton->setCached(true);
    closeButton->setToolTip(tr("Close (<b>Right Click</b>)"));
    closeButton->setIcon(qnSkin->icon("titlebar/exit.png")); // TODO: #Elric
    closeButton->setFixedSize(closeButtonSize, closeButtonSize);
    connect(closeButton, SIGNAL(clicked()), this, SIGNAL(closeTriggered()));

    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addItem(m_textLabel);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Horizontal);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(m_layout);
    layout->addItem(closeButton);
    setLayout(layout);

    updateTailPos();
}

void QnNotificationToolTipWidget::ensureThumbnail(QnImageProvider* provider) {
    if (m_thumbnailLabel || !provider)
        return;

    m_thumbnailLabel = new QnClickableProxyLabel(this);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setClickableButtons(Qt::LeftButton | Qt::RightButton);
    setPaletteColor(m_thumbnailLabel, QPalette::Window, Qt::transparent);
    m_layout->insertItem(0, m_thumbnailLabel);
    connect(m_thumbnailLabel, SIGNAL(clicked(Qt::MouseButton)), this, SLOT(at_thumbnailLabel_clicked(Qt::MouseButton)));

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

void QnNotificationToolTipWidget::clickedNotify(QGraphicsSceneMouseEvent *event) {
    if(event->button() == Qt::RightButton)
        emit closeTriggered();
}

void QnNotificationToolTipWidget::at_thumbnailLabel_clicked(Qt::MouseButton button) {
    if(button == Qt::RightButton) {
        emit closeTriggered();
    } else {
        emit thumbnailClicked();
    }
}

void QnNotificationToolTipWidget::at_provider_imageChanged(const QImage &image) {
    if (!m_thumbnailLabel)
        return;
    m_thumbnailLabel->setPixmap(QPixmap::fromImage(image));
}


// -------------------------------------------------------------------------- //
// QnNotificationWidget
// -------------------------------------------------------------------------- //
QnNotificationWidget::QnNotificationWidget(QGraphicsItem *parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_defaultActionIdx(-1),
    m_notificationLevel(Qn::OtherNotification),
    m_imageProvider(NULL),
    m_inToolTipPositionUpdate(false)
{
    m_color = QnNotificationLevels::notificationColor(m_notificationLevel);

    setClickableButtons(Qt::RightButton | Qt::LeftButton);
    setFrameColor(QColor(110, 110, 110, 128)); // TODO: Same as in workbench_ui. Unify?
    setFrameWidth(1.0);
    setWindowBrush(Qt::transparent);

    m_overlayWidget = new QnFramedWidget(this);
    m_overlayWidget->setFrameStyle(Qt::NoPen);

    m_textLabel = new QnProxyLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_layout->setContentsMargins(margin + colorSignSize.width(), margin, margin, margin);
    m_layout->addItem(m_textLabel);
    m_layout->setStretchFactor(m_textLabel, 1.0);

    setLayout(m_layout);

    m_tooltipWidget = new QnNotificationToolTipWidget(this);
    m_tooltipWidget->setFocusProxy(this);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);
    m_tooltipWidget->installEventFilter(this);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget,            SIGNAL(thumbnailClicked()), this,   SLOT(at_thumbnail_clicked()));
    connect(m_tooltipWidget,            SIGNAL(closeTriggered()),   this,   SIGNAL(closeTriggered()));
    connect(m_tooltipWidget,            SIGNAL(tailPosChanged()),   this,   SLOT(updateToolTipPosition()));
    connect(this,                       SIGNAL(geometryChanged()),  this,   SLOT(updateToolTipPosition()));

    m_toolTipHoverProcessor = new HoverFocusProcessor(this);
    m_toolTipHoverProcessor->addTargetItem(this);
    m_toolTipHoverProcessor->addTargetItem(m_tooltipWidget);
    m_toolTipHoverProcessor->setHoverEnterDelay(250);
    m_toolTipHoverProcessor->setHoverLeaveDelay(250);
    connect(m_toolTipHoverProcessor,    SIGNAL(hoverEntered()),     this,   SLOT(updateToolTipVisibility()));
    connect(m_toolTipHoverProcessor,    SIGNAL(hoverLeft()),        this,   SLOT(updateToolTipVisibility()));

    m_hoverProcessor = new HoverFocusProcessor(this);
    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->addTargetItem(m_tooltipWidget);
    connect(m_hoverProcessor,           SIGNAL(hoverEntered()),     this,   SLOT(updateOverlayVisibility()));
    connect(m_hoverProcessor,           SIGNAL(hoverLeft()),        this,   SLOT(updateOverlayVisibility()));

    updateToolTipPosition();
    updateToolTipVisibility();
    updateOverlayGeometry();
    updateOverlayVisibility(false);
    updateOverlayColor();

    setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

QnNotificationWidget::~QnNotificationWidget() {
    //TODO: #GDM #Business if our sound is playing at the moment - stop it
    return;
}

QString QnNotificationWidget::text() const {
    return m_textLabel->text();
}

void QnNotificationWidget::setText(const QString &text) {
    m_textLabel->setText(text);
}

void QnNotificationWidget::setTooltipText(const QString &text) {
    m_tooltipWidget->setText(text);
}

void QnNotificationWidget::setSound(const QString &soundPath, bool loop) {
    m_soundPath = soundPath;
    if (loop)
        AudioPlayer::playFileAsync(soundPath, this, SLOT(at_loop_sound()));
    else
        AudioPlayer::playFileAsync(soundPath);
}

Qn::NotificationLevel QnNotificationWidget::notificationLevel() const {
    return m_notificationLevel;
}

void QnNotificationWidget::setNotificationLevel(Qn::NotificationLevel notificationLevel) {
    if(m_notificationLevel == notificationLevel)
        return;

    m_notificationLevel = notificationLevel;
    m_color = QnNotificationLevels::notificationColor(m_notificationLevel);

    updateOverlayColor();

    emit notificationLevelChanged();
}

void QnNotificationWidget::setImageProvider(QnImageProvider* provider) {
    m_imageProvider = provider;
}

void QnNotificationWidget::setTooltipEnclosingRect(const QRectF &rect) {
    m_tooltipWidget->setEnclosingGeometry(rect);
}

void QnNotificationWidget::setGeometry(const QRectF &geometry) {
    QSizeF oldSize = size();

    base_type::setGeometry(geometry);

    if(!qFuzzyEquals(size(), oldSize))
        updateOverlayGeometry();
}

void QnNotificationWidget::addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
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

void QnNotificationWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    painter->setPen(QPen(frameBrush(), frameWidth(), frameStyle()));
    painter->setBrush(QBrush(toTransparent(m_color, 0.5)));
    painter->drawRect(QnGeometry::aligned(colorSignSize, rect(), Qt::AlignLeft | Qt::AlignVCenter));

    //TODO: #GDM #Business draw corresponding image
}

void QnNotificationWidget::hideToolTip() {
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(0.0);
}

void QnNotificationWidget::showToolTip() {
    m_tooltipWidget->ensureThumbnail(m_imageProvider);
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(1.0);
}

void QnNotificationWidget::updateToolTipVisibility() {
    if(m_toolTipHoverProcessor->isHovered() && !m_tooltipWidget->text().isEmpty()) {
        showToolTip();
    } else {
        hideToolTip();
    }
}

void QnNotificationWidget::updateToolTipPosition() {
    if(m_inToolTipPositionUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_inToolTipPositionUpdate, true); /* Prevent stack overflow. */

    m_tooltipWidget->updateTailPos();
    m_tooltipWidget->pointTo(QPointF(0, qRound(geometry().height() / 2 )));
}

void QnNotificationWidget::updateOverlayGeometry() {
    m_overlayWidget->setGeometry(QRectF(QPointF(0, 0), size()));
}

void QnNotificationWidget::updateOverlayVisibility(bool animate) {
    if(m_actions.isEmpty()) {
        m_overlayWidget->setOpacity(0);
        return; // TODO: #Elric?
    }

    qreal opacity = m_hoverProcessor->isHovered() ? 1.0 : 0.0;

    if(animate) {
        opacityAnimator(m_overlayWidget, 4.0)->animateTo(opacity);
    } else {
        m_overlayWidget->setOpacity(opacity);
    }
}

void QnNotificationWidget::updateOverlayColor() {
    m_overlayWidget->setWindowBrush(toTransparent(m_color, 0.3));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnNotificationWidget::clickedNotify(QGraphicsSceneMouseEvent *event) {
    Qt::MouseButton button = event->button();

    if(button == Qt::RightButton) {
        emit closeTriggered();
    } else if(button == Qt::LeftButton) {
        if(!m_actions.isEmpty()) {
            ActionData data = m_actions[0]; // TODO: #Elric
            emit actionTriggered(data.action, data.params);
        }
    }
}

void QnNotificationWidget::at_button_clicked() {
    int idx = sender()->property(actionIndexPropertyName).toInt();
    if (m_actions.size() <= idx)
        return;
    ActionData data = m_actions[idx];

    emit actionTriggered(data.action, data.params);
}

void QnNotificationWidget::at_thumbnail_clicked() {
    if (m_defaultActionIdx < 0)
        return;
    if (m_actions.size() <= m_defaultActionIdx)
        return;
    ActionData data = m_actions[m_defaultActionIdx];

    emit actionTriggered(data.action, data.params);
}

void QnNotificationWidget::at_loop_sound() {
    AudioPlayer::playFileAsync(m_soundPath, this, SLOT(at_loop_sound()));
}
