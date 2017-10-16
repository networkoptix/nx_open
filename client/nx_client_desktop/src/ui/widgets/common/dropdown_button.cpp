#include "dropdown_button.h"

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/private/qpushbutton_p.h>

#include <nx/client/core/utils/geometry.h>
#include <ui/style/helper.h>
#include <utils/common/event_processors.h>

using nx::client::core::utils::Geometry;

namespace {

static constexpr int kNoIndex = -1;

} // namespace


QnDropdownButton::QnDropdownButton(QWidget* parent):
    base_type(parent)
{
    auto menu = new QMenu(this);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    setMenu(menu);

    connect(menu, &QMenu::triggered, this, &QnDropdownButton::actionTriggered);

    installEventHandler(menu,
        { QEvent::ActionAdded, QEvent::ActionChanged, QEvent::ActionRemoved },
        this, &QnDropdownButton::actionsUpdated);

    installEventHandler(menu, QEvent::Show, this,
        [this, menu]() { menu->move(menuPosition()); });
}

QnDropdownButton::QnDropdownButton(const QString &text, QWidget* parent):
    QnDropdownButton(parent)
{
    setText(text);
}

QnDropdownButton::QnDropdownButton(const QIcon& icon, const QString& text, QWidget* parent):
    QnDropdownButton(text, parent)
{
    setIcon(icon);
}

Qt::ItemDataRole QnDropdownButton::buttonTextRole() const
{
    return m_buttonTextRole;
}

void QnDropdownButton::setButtonTextRole(Qt::ItemDataRole role)
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

bool QnDropdownButton::displayActionIcon() const
{
    return m_displayActionIcon;
}

void QnDropdownButton::setDisplayActionIcon(bool display)
{
    if (m_displayActionIcon == display)
        return;

    m_displayActionIcon = display;

    updateGeometry();
    update();
}

int QnDropdownButton::count() const
{
    return menu() ? menu()->actions().size() : 0;
}

QAction* QnDropdownButton::action(int index) const
{
    if (!menu() || index < 0)
        return nullptr;

    const auto& actions = menu()->actions();
    if (index >= actions.size())
        return nullptr;

    return actions[index];
}

int QnDropdownButton::indexOf(QAction* action) const
{
    return menu() ? menu()->actions().indexOf(action) : kNoIndex;
}

int QnDropdownButton::currentIndex() const
{
    return m_currentIndex;
}

void QnDropdownButton::setCurrentIndex(int index, bool trigger)
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

QAction* QnDropdownButton::currentAction() const
{
    return m_currentAction;
}

void QnDropdownButton::setCurrentAction(QAction* action, bool trigger)
{
    if (action != m_currentAction)
        setCurrentIndex(indexOf(action), trigger);
}

QSize QnDropdownButton::sizeHint() const
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

void QnDropdownButton::paintEvent(QPaintEvent* event)
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

QString QnDropdownButton::buttonText(QAction* action) const
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

QPoint QnDropdownButton::menuPosition() const
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

void QnDropdownButton::actionsUpdated()
{
    /* Called when an action set of the menu changes. */
    setCurrentIndex(indexOf(m_currentAction), false);
    updateGeometry();
    update();
}

void QnDropdownButton::actionTriggered(QAction* action)
{
    setCurrentAction(action, false);
}

bool QnDropdownButton::isActionSelectable(QAction* action)
{
    return action && action->isEnabled() && !action->isSeparator() && !action->menu();
}
