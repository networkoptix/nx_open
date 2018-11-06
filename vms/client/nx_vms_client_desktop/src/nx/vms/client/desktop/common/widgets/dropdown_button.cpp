#include "dropdown_button.h"

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/private/qpushbutton_p.h>

#include <nx/client/core/utils/geometry.h>
#include <ui/style/helper.h>
#include <utils/common/event_processors.h>

using nx::vms::client::core::Geometry;

namespace {

static constexpr int kNoIndex = -1;

} // namespace


DropdownButton::DropdownButton(QWidget* parent):
    base_type(parent)
{
    auto menu = new QMenu(this);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    setMenu(menu);

    connect(menu, &QMenu::triggered, this, &DropdownButton::actionTriggered);

    installEventHandler(menu,
        { QEvent::ActionAdded, QEvent::ActionChanged, QEvent::ActionRemoved },
        this, &DropdownButton::actionsUpdated);

    installEventHandler(menu, QEvent::Show, this,
        [this, menu]() { menu->move(menuPosition()); });
}

DropdownButton::DropdownButton(const QString &text, QWidget* parent):
    DropdownButton(parent)
{
    setText(text);
}

DropdownButton::DropdownButton(const QIcon& icon, const QString& text, QWidget* parent):
    DropdownButton(text, parent)
{
    setIcon(icon);
}

Qt::ItemDataRole DropdownButton::buttonTextRole() const
{
    return m_buttonTextRole;
}

void DropdownButton::setButtonTextRole(Qt::ItemDataRole role)
{
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            break;

        default:
            role = Qt::DisplayRole;
    }

    if (m_buttonTextRole == role)
        return;

    m_buttonTextRole = role;

    updateGeometry();
    update();
}

bool DropdownButton::displayActionIcon() const
{
    return m_displayActionIcon;
}

void DropdownButton::setDisplayActionIcon(bool display)
{
    if (m_displayActionIcon == display)
        return;

    m_displayActionIcon = display;

    updateGeometry();
    update();
}

int DropdownButton::count() const
{
    return menu() ? menu()->actions().size() : 0;
}

QAction* DropdownButton::action(int index) const
{
    if (!menu() || index < 0)
        return nullptr;

    const auto& actions = menu()->actions();
    if (index >= actions.size())
        return nullptr;

    return actions[index];
}

int DropdownButton::indexOf(QAction* action) const
{
    return menu() ? menu()->actions().indexOf(action) : kNoIndex;
}

int DropdownButton::currentIndex() const
{
    return m_currentIndex;
}

void DropdownButton::setCurrentIndex(int index, bool trigger)
{
    auto action = this->action(index);
    if (!isActionSelectable(action))
        index = kNoIndex;

    if (index == m_currentIndex)
        return;

    m_currentIndex = index;
    m_currentAction = action;

    if (trigger && action)
        action->trigger();

    emit currentChanged(index);
}

QAction* DropdownButton::currentAction() const
{
    return m_currentAction;
}

void DropdownButton::setCurrentAction(QAction* action, bool trigger)
{
    if (action != m_currentAction)
        setCurrentIndex(indexOf(action), trigger);
}

QSize DropdownButton::sizeHint() const
{
    if (!menu())
        return base_type::sizeHint();

    Q_D(const QPushButton);
    if (d->sizeHint.isValid() && d->lastAutoDefault == autoDefault())
        return d->sizeHint;

    d->lastAutoDefault = autoDefault();
    ensurePolished();

    int width = 0;
    int height = 0;

    QStyleOptionButton option;
    initStyleOption(&option);

    if (!icon().isNull())
    {
        static const int kQtIconMargin = 4;
        width += option.iconSize.width() + kQtIconMargin;
        height = qMax(height, option.iconSize.height());
    }

    QFontMetrics metrics = fontMetrics();
    height = qMax(height, metrics.height());

    int maxTextWidth = metrics.size(Qt::TextHideMnemonic, text()).width();

    for (const auto action: menu()->actions())
    {
        if (isActionSelectable(action))
        {
            maxTextWidth = qMax(maxTextWidth,
                metrics.size(Qt::TextHideMnemonic, buttonText(action)).width());
        }
    }

    width += maxTextWidth;

    option.rect.setSize(QSize(width, height));
    width += style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &option, this);

    d->sizeHint = style()->sizeFromContents(QStyle::CT_PushButton, &option, QSize(width, height), this).
        expandedTo(QApplication::globalStrut());

    return d->sizeHint;
}

void DropdownButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);

    const auto action = currentAction();
    if (action)
    {
        option.text = buttonText(action);

        if (m_displayActionIcon)
            option.icon = action->icon();
    }

    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButton, option);
}

QString DropdownButton::buttonText(QAction* action) const
{
    if (!action)
        return QString();

    switch (m_buttonTextRole)
    {
        case Qt::ToolTipRole:
            return action->toolTip();

        case Qt::StatusTipRole:
            return action->statusTip();

        case Qt::WhatsThisRole:
            return action->whatsThis();

        default:
            return action->text();
    }
}

QPoint DropdownButton::menuPosition() const
{
    if (!menu())
        return QPoint();

    auto globalRect = menu()->geometry();

    if (layoutDirection() == Qt::RightToLeft)
        return globalRect.topLeft();

    globalRect.moveTopRight(mapToGlobal(rect().bottomRight() + QPoint(0, 1)));

    const auto desktop = QApplication::desktop();
    return Geometry::movedInto(globalRect, desktop->screenGeometry(this)).topLeft().toPoint();
}

void DropdownButton::actionsUpdated()
{
    /* Called when an action set of the menu changes. */
    setCurrentIndex(indexOf(m_currentAction), false);
    updateGeometry();
    update();
}

void DropdownButton::actionTriggered(QAction* action)
{
    setCurrentAction(action, false);
}

bool DropdownButton::isActionSelectable(QAction* action)
{
    return action && action->isEnabled() && !action->isSeparator() && !action->menu();
}
