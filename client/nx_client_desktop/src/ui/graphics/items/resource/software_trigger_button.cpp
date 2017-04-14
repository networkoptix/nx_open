#include "software_trigger_button.h"

#include <QtWidgets/QApplication>

#include <ui/common/geometry.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/style/icon.h>
#include <ui/style/software_trigger_pixmaps.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

static constexpr int kHoverEnterDelayMs = 0;
static constexpr int kHoverLeaveDelayMs = 250;
static constexpr qreal kToolTipAnimationSpeedFactor = 2.0;
static constexpr qreal kToolTipRoundingRadius = 2.0;
static constexpr qreal kToolTipPadding = 8.0;
static const QSize kDefaultButtonSize(40, 40);

static Qt::Edge invertEdge(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::RightEdge;

        case Qt::RightEdge:
            return Qt::LeftEdge;

        case Qt::TopEdge:
            return Qt::BottomEdge;

        case Qt::BottomEdge:
            return Qt::TopEdge;

        default:
            NX_ASSERT(false);
            return Qt::LeftEdge;
    }
}

} // namespace

QnSoftwareTriggerButton::QnSoftwareTriggerButton(QGraphicsItem* parent):
    base_type(parent),
    m_toolTip(new QnSliderTooltipWidget(this)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(this)),
    m_toolTipEdge(Qt::LeftEdge)
{
    setButtonSize(kDefaultButtonSize);

    connect(this, &QnSoftwareTriggerButton::geometryChanged,
        this, &QnSoftwareTriggerButton::updateToolTipPosition);

    installEventHandler(this, QEvent::PaletteChange, this,
        [this]()
        {
            m_iconDirty = true;
            update();
        });

    m_toolTipHoverProcessor->addTargetItem(this);
    m_toolTipHoverProcessor->addTargetItem(m_toolTip);
    m_toolTipHoverProcessor->setHoverEnterDelay(kHoverEnterDelayMs);
    m_toolTipHoverProcessor->setHoverLeaveDelay(kHoverLeaveDelayMs);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,
        this, &QnSoftwareTriggerButton::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,
        this, &QnSoftwareTriggerButton::updateToolTipVisibility);

    m_toolTip->setOpacity(0.0);
    m_toolTip->setRoundingRadius(kToolTipRoundingRadius);
    m_toolTip->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    updateToolTipTailEdge();
}

QnSoftwareTriggerButton::~QnSoftwareTriggerButton()
{
    if (!isPressed() || !isEnabled())
        return;

    setPressed(false);
    emit released();
}

QString QnSoftwareTriggerButton::toolTip() const
{
    return m_toolTip->text();
}

void QnSoftwareTriggerButton::setToolTip(const QString& toolTip)
{
    m_toolTip->setText(toolTip);
}

Qt::Edge QnSoftwareTriggerButton::toolTipEdge() const
{
    return m_toolTipEdge;
}

void QnSoftwareTriggerButton::setToolTipEdge(Qt::Edge edge)
{
    if (m_toolTipEdge == edge)
        return;

    m_toolTipEdge = edge;
    updateToolTipTailEdge();
}

void QnSoftwareTriggerButton::updateToolTipVisibility()
{
    static constexpr qreal kTransparent = 0.0;
    static constexpr qreal kOpaque = 1.0;

    const bool showToolTip = m_toolTipHoverProcessor->isHovered()
        && !m_toolTip->text().isEmpty()
        && !(m_prolonged && isPressed());

    const qreal targetOpacity = showToolTip ? kOpaque : kTransparent;
    opacityAnimator(m_toolTip, kToolTipAnimationSpeedFactor)->animateTo(targetOpacity);
}

void QnSoftwareTriggerButton::updateToolTipTailEdge()
{
    m_toolTip->setTailEdge(invertEdge(m_toolTipEdge));
    updateToolTipPosition();
}

void QnSoftwareTriggerButton::updateToolTipPosition()
{
    const auto size = this->size();
    switch (m_toolTipEdge)
    {
        case Qt::LeftEdge:
            m_toolTip->pointTo({ -kToolTipPadding, size.height() / 2 });
            break;

        case Qt::RightEdge:
            m_toolTip->pointTo({ size.width() + kToolTipPadding - 1, size.height() / 2 });
            break;

        case Qt::TopEdge:
            m_toolTip->pointTo({ size.width() / 2, -kToolTipPadding });
            break;

        case Qt::BottomEdge:
            m_toolTip->pointTo({ size.width() / 2, size.height() + kToolTipPadding - 1 });
            break;

        default:
            NX_ASSERT(false);
    }
}

QSize QnSoftwareTriggerButton::buttonSize() const
{
    return m_buttonSize;
}

void QnSoftwareTriggerButton::setButtonSize(const QSize& size)
{
    if (m_buttonSize == size)
        return;

    m_buttonSize = size;
    m_iconDirty = true;

    setFixedSize(size);
}

void QnSoftwareTriggerButton::setIcon(const QString& name)
{
    const auto iconName = QnSoftwareTriggerPixmaps::effectivePixmapName(name);
    if (m_iconName == iconName)
        return;

    m_iconName = iconName;
    m_iconDirty = true;
}

bool QnSoftwareTriggerButton::prolonged() const
{
    return m_prolonged;
}

void QnSoftwareTriggerButton::setProlonged(bool value)
{
    if (m_prolonged == value)
        return;

    m_prolonged = value;
    m_iconDirty = true;

    if (m_prolonged)
    {
        connect(this, &QnSoftwareTriggerButton::pressed,
            this, &QnSoftwareTriggerButton::updateToolTipVisibility);
        connect(this, &QnSoftwareTriggerButton::released,
            this, &QnSoftwareTriggerButton::updateToolTipVisibility);
    }
    else
    {
        disconnect(this, &QnSoftwareTriggerButton::pressed,
            this, &QnSoftwareTriggerButton::updateToolTipVisibility);
        disconnect(this, &QnSoftwareTriggerButton::released,
            this, &QnSoftwareTriggerButton::updateToolTipVisibility);
    }
}

void QnSoftwareTriggerButton::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    ensureIcon();
    base_type::paint(painter, option, widget);
}

void QnSoftwareTriggerButton::ensureIcon()
{
    if (!m_iconDirty)
        return;

    m_iconDirty = false;

    if (m_iconName.isEmpty())
    {
        setIcon(QIcon());
        return;
    }

    const auto buttonPixmap = QnSoftwareTriggerPixmaps::pixmapByName(m_iconName);

    const auto generateStatePixmap =
        [this, buttonPixmap](QPalette::ColorRole role) -> QPixmap
        {
            const auto pixelRatio = qApp->devicePixelRatio();
            QPixmap target(m_buttonSize * pixelRatio);
            target.setDevicePixelRatio(pixelRatio);
            target.fill(Qt::transparent);

            const QRect buttonRect(QPoint(), m_buttonSize);

            const auto pixmapRect = QnGeometry::aligned(
                buttonPixmap.size() / buttonPixmap.devicePixelRatio(),
                buttonRect);

            QPainter painter(&target);
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette().brush(role));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawEllipse(buttonRect);
            painter.drawPixmap(pixmapRect, buttonPixmap);

            return target;
        };

    //TODO: #vkutin This is a temporary solution: paint pressed prolonged button with a different background
    const auto pressedRole = m_prolonged
        ? QPalette::Highlight
        : QPalette::Dark;

    QIcon icon;
    icon.addPixmap(generateStatePixmap(QPalette::Window), QIcon::Normal);
    icon.addPixmap(generateStatePixmap(QPalette::Midlight), QIcon::Active);
    icon.addPixmap(generateStatePixmap(pressedRole), QnIcon::Pressed);

    setIcon(icon);
}
