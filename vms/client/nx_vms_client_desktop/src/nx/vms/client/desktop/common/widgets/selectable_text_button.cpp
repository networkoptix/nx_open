#include "selectable_text_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/close_button.h>
#include <ui/common/palette.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <utils/common/event_processors.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

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
    QScopedPointer<CloseButton> deactivateButton;
    QString deactivatedText;
    QString deactivationToolTip;
    bool deactivatedTextSet = false;
    QIcon deactivatedIcon;
    bool deactivatedIconSet = false;
    bool selectable = true;
    bool accented = false;
};

SelectableTextButton::SelectableTextButton(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(this, &QPushButton::clicked, [this]() { setState(State::selected); });
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

    if (value == State::selected && !selectable())
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
    }

    update();
    emit stateChanged(value);
}

bool SelectableTextButton::selectable() const
{
    return d->selectable;
}

void SelectableTextButton::setSelectable(bool value)
{
    if (selectable() == value)
        return;

    d->selectable = value;

    if (d->selectable && state() == State::selected )
        setState(State::unselected);
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
        d->deactivateButton.reset(new CloseButton(this));
        d->deactivateButton->setToolTip(d->deactivationToolTip);

        anchorWidgetToParent(
            d->deactivateButton.data(), Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge, {0, 0, 0, 1});

        connect(d->deactivateButton.data(), &CloseButton::clicked,
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

bool SelectableTextButton::accented() const
{
    return d->accented;
}

void SelectableTextButton::setAccented(bool accented)
{
    d->accented = accented;
    update();
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
    const auto textSize = QFontMetrics(font()).size(0, effectiveText());
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

QSize SelectableTextButton::minimumSizeHint() const
{
    return QSize(style::Metrics::kMinimumButtonWidth, kMinimumHeight);
}

bool SelectableTextButton::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Shortcut:
        case QEvent::KeyPress:
            if (menu())
                return base_type::event(event);
            [[fallthrough]];
        case QEvent::Enter:
        case QEvent::Leave:
        {
            // QAbstractButton forces repaint before emitting clicked.
            // We change state on clicked, therefore have to block updates until then.
            auto updateGuard = nx::utils::makeScopeGuard(
                [this]() { setUpdatesEnabled(true); });
            setUpdatesEnabled(false);
            return base_type::event(event);
        }

        default:
            return base_type::event(event);
    }
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
    const bool hovered = isEnabled() && !isDown()
        && (underMouse() || (d->deactivateButton && d->deactivateButton->underMouse()));

    if (d->state != State::deactivated)
    {
        if (d->state == State::selected)
        {
            painter.setPen(palette.color(QPalette::Shadow));
            painter.setBrush(palette.dark());
        }
        else
        {
            NX_ASSERT(d->state == State::unselected);
            painter.setPen(Qt::NoPen);

            if (d->accented)
                painter.setBrush(hovered ? palette.light() : palette.highlight());
            else
                painter.setBrush(hovered ? palette.midlight() : palette.window());
        }

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

        const auto iconMode = d->state == State::deactivated
            ? (hovered ? QIcon::Active : QIcon::Normal)
            : ((d->accented && d->state != State::selected) ? QIcon::Selected : QIcon::Normal);

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

    switch (d->state)
    {
        case State::deactivated:
            painter.setPen(palette.color(hovered ? QPalette::WindowText : QPalette::Text));
            break;

        case State::unselected:
            painter.setPen(palette.color(d->accented
                ? QPalette::HighlightedText
                : QPalette::WindowText));
            break;

        case State::selected:
            painter.setPen(palette.color(QPalette::WindowText));
            break;

        default:
            NX_ASSERT(false, "Should never get here!");
    }

    const auto fullText = effectiveText();
    const auto elidedText = fontMetrics().elidedText(fullText, Qt::ElideRight, rect.width());
    painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, elidedText);
}

} // namespace nx::vms::client::desktop
