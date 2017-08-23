#include "selectable_text_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <ui/common/widget_anchor.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <utils/common/event_processors.h>
#include <nx/utils/raii_guard.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

namespace {

static constexpr int kTextIndent = 6;
static constexpr int kIconIndent = 4;
static constexpr int kIconPadding = 2;
static constexpr int kDeactivateButtonSize = 22;
static constexpr int kRoundingRadius = 2;
static constexpr int kMinimumHeight = 24;

auto stateColorGroup(SelectableTextButton::State state)
{
    return state == SelectableTextButton::State::selected
        ? QPalette::Active
        : QPalette::Inactive;
}

} // namespace

struct SelectableTextButton::Private
{
    State state = State::unselected;
    QScopedPointer<QToolButton> deactivateButton;
    QString deactivatedText;
    QString deactivationToolTip;
    bool deactivatedTextSet = false;
    QIcon deactivatedIcon;
    bool deactivatedIconSet = false;
};

SelectableTextButton::SelectableTextButton(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(this, &QPushButton::clicked, [this]() { setState(State::selected); });

    installEventHandler(this, QEvent::PaletteChange, this,
        &SelectableTextButton::updateDeactivateButtonPalette);
    connect(this, &QPushButton::pressed, this,
        &SelectableTextButton::updateDeactivateButtonPalette);
    connect(this, &QPushButton::released, this,
        &SelectableTextButton::updateDeactivateButtonPalette);
}

SelectableTextButton::~SelectableTextButton()
{
}

SelectableTextButton::State SelectableTextButton::state() const
{
    return d->state;
}

void SelectableTextButton::setState(State value)
{
    if (d->state == value)
        return;

    if (value == State::deactivated && !deactivatable())
        return;

    d->state = value;

    if (d->deactivateButton)
    {
        const bool hidden = value == State::deactivated;
        if (hidden != d->deactivateButton->isHidden())
        {
            d->deactivateButton->setHidden(hidden);
            updateGeometry();
        }

        if (!hidden)
            updateDeactivateButtonPalette();
    }

    update();
    emit stateChanged(value);
}

bool SelectableTextButton::deactivatable() const
{
    return !d->deactivateButton.isNull();
}

void SelectableTextButton::setDeactivatable(bool value)
{
    if (deactivatable() == value)
        return;

    if (value)
    {
        d->deactivateButton.reset(new QToolButton(this));
        d->deactivateButton->setIcon(qnSkin->icon(lit("buttons/clear.png")));
        d->deactivateButton->setFixedSize({ kDeactivateButtonSize, kDeactivateButtonSize });
        d->deactivateButton->setToolTip(d->deactivationToolTip);
        updateDeactivateButtonPalette();

        auto anchor = new QnWidgetAnchor(d->deactivateButton.data());
        anchor->setEdges(Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);
        anchor->setMargins(0, 1, 1, 1);

        connect(d->deactivateButton.data(), &QToolButton::clicked,
            [this]() { setState(State::deactivated); });

        setState(State::deactivated); //< Sets deactivated state by default.
    }
    else
    {
        d->deactivateButton.reset();

        if (d->state == State::deactivated)
            setState(State::unselected);
    }

    updateGeometry();
}

void SelectableTextButton::deactivate()
{
    setState(State::deactivated);
}

QString SelectableTextButton::deactivatedText() const
{
    return d->deactivatedTextSet ? d->deactivatedText : text();
}

void SelectableTextButton::setDeactivatedText(const QString& value)
{
    if (d->deactivatedTextSet && value == d->deactivatedText)
        return;

    d->deactivatedText = value;
    d->deactivatedTextSet = true;

    if (d->state == State::deactivated)
        updateGeometry();
}

void SelectableTextButton::unsetDeactivatedText()
{
    if (!d->deactivatedTextSet)
        return;

    d->deactivatedText = QString();
    d->deactivatedTextSet = false;

    if (d->state == State::deactivated)
        updateGeometry();
}

QString SelectableTextButton::effectiveText() const
{
    return d->state == State::deactivated ? deactivatedText() : text();
}

QIcon SelectableTextButton::deactivatedIcon() const
{
    return d->deactivatedIconSet ? d->deactivatedIcon : icon();
}

void SelectableTextButton::setDeactivatedIcon(const QIcon& value)
{
    d->deactivatedIcon = value;
    d->deactivatedIconSet = true;

    if (d->state == State::deactivated)
        updateGeometry();
}

void SelectableTextButton::unsetDeactivatedIcon()
{
    if (!d->deactivatedIconSet)
        return;

    d->deactivatedIcon = QIcon();
    d->deactivatedIconSet = false;

    if (d->state == State::deactivated)
        updateGeometry();
}

QIcon SelectableTextButton::effectiveIcon() const
{
    return d->state == State::deactivated ? deactivatedIcon() : icon();
}

QString SelectableTextButton::deactivationToolTip() const
{
    return d->deactivationToolTip;
}

void SelectableTextButton::setDeactivationToolTip(const QString& value)
{
    if (d->deactivationToolTip == value)
        return;

    d->deactivationToolTip = value;
    if (d->deactivateButton)
        d->deactivateButton->setToolTip(d->deactivationToolTip);
}

QSize SelectableTextButton::sizeHint() const
{
    return minimumSizeHint();
}

QSize SelectableTextButton::minimumSizeHint() const
{
    const auto textSize = QFontMetrics(font()).size(Qt::TextHideMnemonic, effectiveText());
    const auto iconSize = QnSkin::maximumSize(effectiveIcon());

    int extraWidth = iconSize.isValid()
        ? iconSize.width() + kIconIndent + kIconPadding
        : kTextIndent;

    if (d->deactivateButton && !d->deactivateButton->isHidden())
        extraWidth += d->deactivateButton->width() + 2;
    else
        extraWidth += kTextIndent;

    return QSize(
        textSize.width() + extraWidth,
        qMax(kMinimumHeight, qMax(textSize.height(), iconSize.height()) + 2));
}

bool SelectableTextButton::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Shortcut:
        case QEvent::KeyPress:
        {
            // QAbstractButton forces repaint before emitting clicked.
            // We change state on clicked, therefore have to block updates until then.
            QnRaiiGuard updateGuard([this]() { setUpdatesEnabled(true); });
            setUpdatesEnabled(false);

            return base_type::event(event);
        }

        default:
            return base_type::event(event);
    }
}

void SelectableTextButton::updateDeactivateButtonPalette()
{
    if (!d->deactivateButton)
        return;

    setPaletteColor(d->deactivateButton.data(), QPalette::Window, isDown()
        ? Qt::transparent
        : palette().color(stateColorGroup(d->state), QPalette::Window));
}

void SelectableTextButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    if (!isEnabled())
        painter.setOpacity(style::Hints::kDisabledItemOpacity);

    // Paint panel.

    auto palette = this->palette();
    palette.setCurrentColorGroup(stateColorGroup(d->state));

    auto rect = this->rect();
    const bool hovered = isEnabled() && underMouse()
        && (d->deactivateButton.isNull() || !d->deactivateButton->underMouse());

    if (d->state != State::deactivated)
    {
        painter.setPen(d->state == State::selected
            ? QPen(palette.color(QPalette::Shadow))
            : Qt::NoPen);

        painter.setBrush(isDown()
            ? palette.dark()
            : (hovered ? palette.midlight() : palette.window()));

        painter.drawRoundedRect(rect, kRoundingRadius, kRoundingRadius);
    }

    enum class Side { left, right };
    const auto adjustRect =
        [this](QRect& rect, Side side, int amount)
        {
            const bool rightSide = (side == Side::right) != (layoutDirection() == Qt::RightToLeft);
            if (rightSide)
                rect.setRight(rect.right() - amount);
            else
                rect.setLeft(rect.left() + amount);
        };

    // Paint icon.

    const auto icon = effectiveIcon();
    adjustRect(rect, Side::left, icon.isNull() ? kTextIndent : kIconIndent);

    if (!icon.isNull())
    {
        const auto iconSize = QnSkin::maximumSize(icon);
        const auto iconRect = QStyle::alignedRect(layoutDirection(),
            Qt::AlignLeft | Qt::AlignVCenter, iconSize, rect);

        const auto iconMode = hovered && d->state == State::deactivated
            ? QIcon::Active
            : QIcon::Normal;

        const auto iconState = d->state == State::selected
            ? QIcon::On
            : QIcon::Off;

        icon.paint(&painter, iconRect, Qt::AlignCenter, iconMode, iconState);
        adjustRect(rect, Side::left, iconSize.width() + kIconPadding);
    }

    // Paint text.

    const int rightIndent = d->deactivateButton && !d->deactivateButton->isHidden()
        ? d->deactivateButton->width() + 2
        : kTextIndent;

    adjustRect(rect, Side::right, rightIndent);

    painter.setPen(palette.color(d->state != State::deactivated || hovered
        ? QPalette::WindowText
        : QPalette::Text));

    painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, effectiveText());
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
