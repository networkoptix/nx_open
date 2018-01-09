#include "notification_widget.h"

#include <limits>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QPushButton>

#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/common/geometry.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/media/audio_player.h>
#include <nx/client/desktop/image_providers/image_provider.h>

#include <nx/fusion/model_functions.h>

using namespace nx::client::desktop::ui;

namespace {

QString getFullAlias(const QString& postfix)
{
    static const auto kNotificationWidgetAlias = lit("notification_widget");
    return lit("%1_%2").arg(kNotificationWidgetAlias, postfix);
};

const char* kActionIndexPropertyName = "_qn_actionIndex";

static constexpr int kColorSignMargin = 4;
static constexpr int kHorizontalMargin = 12;
static constexpr int kExtraHorizontalMargin = 28;
static constexpr int kVerticalMargin = 6;
static constexpr int kNotificationIconWidth = 24;

} // namespace

// -------------------------------------------------------------------------- //
// QnNotificationToolTipWidget
// -------------------------------------------------------------------------- //
QnNotificationToolTipWidget::QnNotificationToolTipWidget(QGraphicsItem* parent) :
    base_type(parent)
{
}

void QnNotificationToolTipWidget::updateTailPos()
{
    if (m_enclosingRect.isNull())
        return;

    if (!parentItem())
        return;

    if (!parentItem()->parentItem())
        return;

    QRectF rect = this->rect();
    QGraphicsItem* list = parentItem()->parentItem();

    const auto parentY = parentItem()->mapToItem(list, m_pointTo).y();
    const auto tailX = qRound(rect.right() + tailLength());

    const auto toolTipHeight = rect.height();
    const auto halfHeight = toolTipHeight / 2;

    const auto spaceToTop = parentY - m_enclosingRect.top();
    const auto spaceToBottom = m_enclosingRect.bottom() - parentY;

    static const int kOffset = tailWidth() / 2;

    // Check if we are too close to the top (or there is not enough space in any case)
    if (spaceToTop < halfHeight || m_enclosingRect.height() < toolTipHeight)
    {
        const auto tailY = qRound(rect.top() + spaceToTop - kOffset);
        setTailPos(QPointF(tailX, tailY));
    }
    // Check if we are too close to the bottom
    else if (spaceToBottom < halfHeight)
    {
        const auto tailY = qRound(rect.bottom() - spaceToBottom + kOffset);
        setTailPos(QPointF(tailX, tailY));
    }
    // Optimal position
    else
    {
        const auto tailY = qRound((rect.top() + rect.bottom()) / 2);
        setTailPos(QPointF(tailX, tailY));
    }

    // cannot call base_type as it is reimplemented
    QnToolTipWidget::pointTo(m_pointTo);
}

void QnNotificationToolTipWidget::setEnclosingGeometry(const QRectF& enclosingGeometry)
{
    m_enclosingRect = enclosingGeometry;
    updateTailPos();
}

// -------------------------------------------------------------------------- //
// QnNotificationWidget
// -------------------------------------------------------------------------- //
QnNotificationWidget::QnNotificationWidget(QGraphicsItem* parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_defaultActionIdx(-1),
    m_verticalLayout(new QGraphicsLinearLayout(Qt::Vertical)),
    m_primaryLayout(new QGraphicsLinearLayout(Qt::Horizontal)),
    m_textLabel(new QnProxyLabel(this)),
    m_closeButton(new QnImageButtonWidget(this)),
    m_notificationLevel(QnNotificationLevel::Value::OtherNotification),
    m_imageProvider(nullptr),
    m_color(QnNotificationLevel::notificationColor(m_notificationLevel)),
    m_tooltipWidget(new QnNotificationToolTipWidget(this)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(this)),
    m_hoverProcessor(new HoverFocusProcessor(this)),
    m_pendingPositionUpdate(false),
    m_instantPositionUpdate(false),
    m_inToolTipPositionUpdate(false)
{
    setClickableButtons(Qt::RightButton | Qt::LeftButton);

    const auto closeButtonIcon = qnSkin->icon(lit("events/notification_close.png"));
    const auto closeButtonSize = QnSkin::maximumSize(closeButtonIcon);
    m_closeButton->setIcon(closeButtonIcon);
    m_closeButton->setFixedSize(closeButtonSize);
    m_closeButton->setVisible(false);
    connect(m_closeButton, &QnImageButtonWidget::clicked, this, &QnNotificationWidget::closeTriggered);

    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    connect(m_textLabel, &QnProxyLabel::linkActivated, this, &QnNotificationWidget::linkActivated);

    m_primaryLayout->setContentsMargins(0, 0, closeButtonSize.width(), 0);
    m_primaryLayout->addItem(m_textLabel);
    m_primaryLayout->setStretchFactor(m_textLabel, 1.0);

    m_verticalLayout->setSpacing(8);
    m_verticalLayout->setContentsMargins(kHorizontalMargin, kVerticalMargin, 0, kVerticalMargin);
    m_verticalLayout->addItem(m_primaryLayout);
    setLayout(m_verticalLayout);

    m_tooltipWidget->setFocusProxy(this);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);
    m_tooltipWidget->installEventFilter(this);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget, &QnNotificationToolTipWidget::thumbnailClicked, this, &QnNotificationWidget::triggerDefaultAction);
    connect(m_tooltipWidget, &QnNotificationToolTipWidget::tailPosChanged,   this, &QnNotificationWidget::updateToolTipPosition);
    connect(this,            &QnNotificationWidget::geometryChanged,         this, &QnNotificationWidget::updateToolTipPosition);

    m_toolTipHoverProcessor->addTargetItem(this);
    m_toolTipHoverProcessor->addTargetItem(m_tooltipWidget);
    m_toolTipHoverProcessor->setHoverEnterDelay(250);
    m_toolTipHoverProcessor->setHoverLeaveDelay(250);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,     this,   &QnNotificationWidget::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,        this,   &QnNotificationWidget::updateToolTipVisibility);

    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->addTargetItem(m_tooltipWidget);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            if (m_closeButtonAvailable &&
                m_notificationLevel != QnNotificationLevel::Value::NoNotification)
            {
                m_closeButton->show();
            }
        });
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            m_closeButton->hide();
        });

    updateToolTipPosition();
    updateToolTipVisibility();
    updateLabelPalette();

    setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

QnNotificationWidget::~QnNotificationWidget()
{
    // TODO: #GDM #Business if our sound is playing at the moment - stop it
}

QString QnNotificationWidget::text() const
{
    return m_textLabel->text();
}

void QnNotificationWidget::setText(const QString& text)
{
    m_textLabel->setText(text);
}

void QnNotificationWidget::setTooltipText(const QString& text)
{
    m_tooltipWidget->setText(text);
}

void QnNotificationWidget::setSound(const QString& soundPath, bool loop)
{
    m_soundPath = soundPath;
    if (loop)
        AudioPlayer::playFileAsync(soundPath, this, SLOT(at_loop_sound()));
    else
        AudioPlayer::playFileAsync(soundPath);
}

QnNotificationLevel::Value QnNotificationWidget::notificationLevel() const
{
    return m_notificationLevel;
}

void QnNotificationWidget::setNotificationLevel(QnNotificationLevel::Value notificationLevel)
{
    if (m_notificationLevel == notificationLevel)
        return;

    m_notificationLevel = notificationLevel;
    m_color = QnNotificationLevel::notificationColor(m_notificationLevel);
    updateLabelPalette();

    emit notificationLevelChanged();
}

void QnNotificationWidget::setImageProvider(QnImageProvider* provider)
{
    m_imageProvider = provider;
    m_tooltipWidget->setImageProvider(provider);
}

void QnNotificationWidget::setTooltipEnclosingRect(const QRectF& rect)
{
    m_tooltipWidget->setEnclosingGeometry(rect);
}

void QnNotificationWidget::setGeometry(const QRectF& geometry)
{
    base_type::setGeometry(geometry);

    auto label = m_textLabel->widget();
    m_textLabel->setPreferredHeight(label->heightForWidth(label->width()));

    QRectF buttonGeometry(QPointF(), m_closeButton->size());
    buttonGeometry.moveTopRight(rect().topRight() + QPointF(0, 1));
    m_closeButton->setGeometry(buttonGeometry);
}

void QnNotificationWidget::addActionButton(
    const QIcon& icon,
    ActionType actionId,
    const ParametersType& parameters,
    bool defaultAction)
{
    QnImageButtonWidget* button = new QnImageButtonWidget(this);
    button->setAcceptHoverEvents(false);
    button->setIcon(icon);
    button->setProperty(kActionIndexPropertyName, m_actions.size());
    button->setFixedSize(QnSkin::maximumSize(icon));

    if (m_defaultActionIdx < 0 || defaultAction)
        m_defaultActionIdx = m_actions.size();

    QGraphicsLinearLayout* layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    layout->setMinimumWidth(kNotificationIconWidth);
    layout->setSpacing(0.0);
    layout->addItem(button);
    layout->setAlignment(button, Qt::AlignHCenter);
    layout->addStretch();
    m_primaryLayout->insertItem(0, layout);

    connect(button, &QnImageButtonWidget::clicked, this, [this, actionId, parameters]()
    {
        emit buttonClicked(getFullAlias(QnLexical::serialized(actionId)));
        emit actionTriggered(actionId, parameters);
    });
    m_actions << ActionData(actionId, parameters); //still required for thumbnails click and base notification click
}

void QnNotificationWidget::addTextButton(
    const QIcon& icon,
    const QString& text,
    const ButtonHandler& handler)
{
    auto button = new QPushButton();
    if (!text.isEmpty())
        button->setText(text);
    if (!icon.isNull())
        button->setIcon(icon);

    connect(button, &QAbstractButton::clicked, this,
        [this, handler]()
        {
            if (handler)
                handler();
        });

    auto proxy = new QGraphicsProxyWidget();
    proxy->setWidget(button);

    auto rowLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    rowLayout->setContentsMargins(kNotificationIconWidth + m_primaryLayout->spacing(),
        0,
        kHorizontalMargin,
        0);
    rowLayout->addItem(proxy);
    rowLayout->addStretch(1);
    m_verticalLayout->addItem(rowLayout);
}

bool QnNotificationWidget::isCloseButtonAvailable() const
{
    return m_closeButtonAvailable;
}

void QnNotificationWidget::setCloseButtonAvailable(bool value)
{
    m_closeButtonAvailable = value;
    if (value)
        m_closeButton->show();
    else
        m_closeButton->hide();
}

void QnNotificationWidget::triggerDefaultAction()
{
    if (m_defaultActionIdx < 0)
        return;

    NX_ASSERT(m_defaultActionIdx < m_actions.size());
    const auto actionData = m_actions[m_defaultActionIdx];

    emit actionTriggered(actionData.action, actionData.params);
}

void QnNotificationWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (m_hoverProcessor->isHovered())
        painter->fillRect(rect().adjusted(1, 1, 0, 0), palette().shadow());

    QRectF rect = this->rect().adjusted(0.5, 0.5, -0.5, 0.5);
    base_type::paint(painter, option, widget);

    QnScopedPainterPenRollback penRollback(painter, m_color);

    painter->drawLine(
        QPointF(rect.left(), rect.top() + kColorSignMargin),
        QPointF(rect.left(), rect.bottom() - kColorSignMargin));

    rect.adjust(kHorizontalMargin, 0, -kHorizontalMargin, 0);

    painter->setPen(palette().color(QPalette::Dark));

    painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    // TODO: #GDM #Business draw corresponding image
}

void QnNotificationWidget::hideToolTip()
{
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(0.0);
}

void QnNotificationWidget::showToolTip()
{
    if (m_imageProvider)
        m_imageProvider->loadAsync();
    m_tooltipWidget->setThumbnailVisible(!m_imageProvider.isNull());
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(1.0);
}

void QnNotificationWidget::updateLabelPalette()
{
    auto textColorRole = m_notificationLevel == QnNotificationLevel::Value::NoNotification
        ? QPalette::AlternateBase
        : QPalette::WindowText;

    setPaletteColor(m_textLabel, QPalette::WindowText, palette().color(textColorRole));
}

void QnNotificationWidget::updateToolTipVisibility()
{
    if (m_toolTipHoverProcessor->isHovered() && !m_tooltipWidget->text().isEmpty())
        showToolTip();
    else
        hideToolTip();
}

void QnNotificationWidget::updateToolTipPosition()
{
    if (m_inToolTipPositionUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_inToolTipPositionUpdate, true); /* Prevent stack overflow. */

    m_tooltipWidget->updateTailPos();
    m_tooltipWidget->pointTo(QPointF(0, qRound(geometry().height() / 2 )));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnNotificationWidget::clickedNotify(QGraphicsSceneMouseEvent* event)
{
    Qt::MouseButton button = event->button();

    if (button == Qt::LeftButton)
        triggerDefaultAction();
}

void QnNotificationWidget::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    switch (event->type())
    {
        case QEvent::PaletteChange:
            updateLabelPalette();
            break;
        default:
            break;
    }
}

void QnNotificationWidget::at_loop_sound()
{
    AudioPlayer::playFileAsync(m_soundPath, this, SLOT(at_loop_sound()));
}
