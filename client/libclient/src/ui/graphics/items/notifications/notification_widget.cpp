#include "notification_widget.h"

#include <limits>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

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
#include <utils/image_provider.h>

#include <nx/fusion/model_functions.h>

namespace {

QString getFullAlias(const QString& postfix)
{
    static const auto kNotificationWidgetAlias = lit("notification_widget");
    return lit("%1_%2").arg(kNotificationWidgetAlias, postfix);
};

const char* kActionIndexPropertyName = "_qn_actionIndex";

const int kColorSignMargin = 4;
const int kHorizontalMargin = 12;
const int kVerticalMargin = 6;

} // anonymous namespace

// -------------------------------------------------------------------------- //
// QnNotificationToolTipWidget
// -------------------------------------------------------------------------- //
QnNotificationToolTipWidget::QnNotificationToolTipWidget(QGraphicsItem* parent) :
    base_type(parent),
    m_thumbnailLabel(nullptr)
{
    setClickableButtons(Qt::RightButton);

    m_textLabel = new QnClickableProxyLabel(this);
    m_textLabel->setAlignment(Qt::AlignLeft);
    m_textLabel->setWordWrap(true);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    m_layout = new QGraphicsLinearLayout(Qt::Vertical);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addItem(m_textLabel);

    setLayout(m_layout);

    updateTailPos();
}

void QnNotificationToolTipWidget::ensureThumbnail(QnImageProvider* provider)
{
    if (m_thumbnailLabel || !provider)
        return;

    m_thumbnailLabel = new QnClickableProxyLabel(this);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setClickableButtons(Qt::LeftButton | Qt::RightButton);
    setPaletteColor(m_thumbnailLabel, QPalette::Window, Qt::transparent);
    m_layout->addItem(m_thumbnailLabel);
    connect(m_thumbnailLabel, SIGNAL(clicked(Qt::MouseButton)), this, SLOT(at_thumbnailLabel_clicked(Qt::MouseButton)));

    if (!provider->image().isNull())
    {
        m_thumbnailLabel->setPixmap(QPixmap::fromImage(provider->image()));
    }
    else
    {
        m_thumbnailLabel->setPixmap(qnSkin->pixmap("events/thumb_loading.png",
            QSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation, true));
    }

    connect(provider, &QnImageProvider::imageChanged, this, [this](const QImage& image)
    {
        m_thumbnailLabel->setPixmap(QPixmap::fromImage(image));
    });

    provider->loadAsync();
}

QString QnNotificationToolTipWidget::text() const
{
    return m_textLabel->text();
}

void QnNotificationToolTipWidget::setText(const QString& text)
{
    m_textLabel->setText(text);
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

    // half of the tooltip height in coordinates of enclosing rect
    qreal halfHeight = mapRectToItem(list, rect).height() / 2;

    qreal parentPos = parentItem()->mapToItem(list, m_pointTo).y();

    if (parentPos - halfHeight < m_enclosingRect.top())
        setTailPos(QPointF(qRound(rect.right() + tailLength()), qRound(rect.top() + tailWidth())));
    else
    if (parentPos + halfHeight > m_enclosingRect.bottom())
        setTailPos(QPointF(qRound(rect.right() + tailLength()), qRound(rect.bottom() - tailWidth())));
    else
        setTailPos(QPointF(qRound(rect.right() + tailLength()), qRound((rect.top() + rect.bottom()) / 2)));

    base_type::pointTo(m_pointTo);
}

void QnNotificationToolTipWidget::setEnclosingGeometry(const QRectF& enclosingGeometry)
{
    m_enclosingRect = enclosingGeometry;
    updateTailPos();
}

void QnNotificationToolTipWidget::pointTo(const QPointF& pos)
{
    m_pointTo = pos;
    base_type::pointTo(pos);
    updateTailPos();
}

void QnNotificationToolTipWidget::clickedNotify(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
        emit closeTriggered();
}

void QnNotificationToolTipWidget::at_thumbnailLabel_clicked(Qt::MouseButton button)
{
    if (button == Qt::RightButton)
        emit closeTriggered();
    else
        emit thumbnailClicked();
}

// -------------------------------------------------------------------------- //
// QnNotificationWidget
// -------------------------------------------------------------------------- //
QnNotificationWidget::QnNotificationWidget(QGraphicsItem* parent, Qt::WindowFlags flags) :
    base_type(parent, flags),
    m_defaultActionIdx(-1),
    m_layout(new QGraphicsLinearLayout(Qt::Horizontal)),
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

    m_closeButton->setIcon(qnSkin->icon(lit("events/notification_close.png")));
    m_closeButton->setFixedSize(QnSkin::maximumSize(m_closeButton->icon()));
    m_closeButton->setVisible(false);
    connect(m_closeButton, SIGNAL(clicked()), this, SIGNAL(closeTriggered()));

    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setPaletteColor(m_textLabel, QPalette::Window, Qt::transparent);

    m_layout->setContentsMargins(kHorizontalMargin, kVerticalMargin, kHorizontalMargin, kVerticalMargin);
    m_layout->addItem(m_textLabel);
    m_layout->setStretchFactor(m_textLabel, 1.0);

    setLayout(m_layout);

    m_tooltipWidget->setFocusProxy(this);
    m_tooltipWidget->setOpacity(0.0);
    m_tooltipWidget->setAcceptHoverEvents(true);
    m_tooltipWidget->installEventFilter(this);
    m_tooltipWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    connect(m_tooltipWidget, &QnNotificationToolTipWidget::buttonClicked,    this, &QnNotificationWidget::buttonClicked);
    connect(m_tooltipWidget, &QnNotificationToolTipWidget::thumbnailClicked, this, &QnNotificationWidget::at_thumbnail_clicked);
    connect(m_tooltipWidget, &QnNotificationToolTipWidget::closeTriggered,   this, &QnNotificationWidget::closeTriggered);
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
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverEntered, this, [this]() { m_closeButton->show(); });
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft,    this, [this]() { m_closeButton->hide(); });

    updateToolTipPosition();
    updateToolTipVisibility();

    setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

QnNotificationWidget::~QnNotificationWidget()
{
    //TODO: #GDM #Business if our sound is playing at the moment - stop it
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

    emit notificationLevelChanged();
}

void QnNotificationWidget::setImageProvider(QnImageProvider* provider)
{
    m_imageProvider = provider;
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
    const QIcon& icon, const QString& /*tooltip*/, QnActions::IDType actionId,
    const QnActionParameters& parameters, bool defaultAction)
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
    layout->setSpacing(0.0);
    layout->addItem(button);
    layout->addStretch(1);
    m_layout->insertItem(0, layout);

    connect(button, &QnImageButtonWidget::clicked, this, [this, actionId, parameters]()
    {
        emit buttonClicked(getFullAlias(QnLexical::serialized(actionId)));
        emit actionTriggered(actionId, parameters);
    });
    m_actions << ActionData(actionId, parameters); //still required for thumbnails click and base notification click
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

    //TODO: #GDM #Business draw corresponding image
}

void QnNotificationWidget::hideToolTip()
{
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(0.0);
}

void QnNotificationWidget::showToolTip()
{
    m_tooltipWidget->ensureThumbnail(m_imageProvider);
    opacityAnimator(m_tooltipWidget, 2.0)->animateTo(1.0);
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

    if (button == Qt::RightButton)
    {
        emit closeTriggered();
    }
    else if (button == Qt::LeftButton)
    {
        if (!m_actions.isEmpty())
        {
            ActionData data = m_actions[0]; // TODO: #Elric
            emit actionTriggered(data.action, data.params);
        }
    }
}

void QnNotificationWidget::at_thumbnail_clicked()
{
    if (m_defaultActionIdx < 0)
        return;

    if (m_actions.size() <= m_defaultActionIdx)
        return;

    ActionData data = m_actions[m_defaultActionIdx];
    emit actionTriggered(data.action, data.params);
}

void QnNotificationWidget::at_loop_sound()
{
    AudioPlayer::playFileAsync(m_soundPath, this, SLOT(at_loop_sound()));
}
