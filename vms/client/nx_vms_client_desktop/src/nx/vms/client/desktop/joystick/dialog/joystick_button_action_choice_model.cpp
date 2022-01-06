// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_button_action_choice_model.h"

#include <QtCore/qhashfunctions.h>
#include <QtGui/QAction>

#include <utils/math/math.h>

#include "../settings/descriptors.h"

namespace nx::vms::client::desktop::joystick {

namespace {

const QChar kArrowsSymbol(0x21C4);

} // namespace

struct ButtonInfo
{
    QString name;
    bool withModifier = false;

    bool operator==(const ButtonInfo& other) const = default;
};

uint qHash(ButtonInfo key, uint seed = 0) noexcept
{
    QtPrivate::QHashCombine hash;

    seed = hash(seed, key.name);
    return hash(seed, key.withModifier);
}

struct JoystickButtonActionChoiceModel::Private
{
    JoystickButtonActionChoiceModel* const q;

    const bool withModifier;
    bool openLayoutChoiceEnabled = true;
    bool openLayoutChoiceVisible = true;
    QVector<ActionInfo> info;
    QMap<ui::action::IDType, QSet<ButtonInfo>> actionToButtons;

    Private(JoystickButtonActionChoiceModel* parent, bool withModifier):
        q(parent),
        withModifier(withModifier)
    {
    }

    void formContent()
    {
        info = itemsBeforeOpenLayoutItem();

        if (openLayoutChoiceVisible)
            info += openLayoutItems();

        info += itemsAfterOpenLayoutItem();

        if (withModifier)
            info += modifierItems();
    }
};

JoystickButtonActionChoiceModel::ActionInfo JoystickButtonActionChoiceModel::separator()
{
    return {};
}

QVector<JoystickButtonActionChoiceModel::ActionInfo>
    JoystickButtonActionChoiceModel::itemsBeforeOpenLayoutItem()
{
    return {
        {ui::action::NoAction, QString("- %1 -").arg(tr("No Action"))},
        separator(),
        {ui::action::PtzFocusInAction, tr("Focus Near")},
        {ui::action::PtzFocusOutAction, tr("Focus Far")},
        {ui::action::PtzFocusAutoAction, tr("Autofocus")},
        separator(),
        {ui::action::PtzActivateByHotkeyAction, tr("Go to PTZ Position")}
    };
}

QVector<JoystickButtonActionChoiceModel::ActionInfo>
    JoystickButtonActionChoiceModel::openLayoutItems()
{
    return {
        separator(),
        {ui::action::OpenInNewTabAction, tr("Open Layout")}
    };
}

QVector<JoystickButtonActionChoiceModel::ActionInfo>
    JoystickButtonActionChoiceModel::itemsAfterOpenLayoutItem()
{
    return {
        separator(),
        {ui::action::FullscreenResourceAction, tr("Set to Fullscreen")},
        separator(),
        {ui::action::GoToNextItemAction, tr("Next Camera on Layout")},
        {ui::action::GoToPreviousItemAction, tr("Previous Camera on Layout")}
    };
}

QVector<JoystickButtonActionChoiceModel::ActionInfo>
    JoystickButtonActionChoiceModel::modifierItems()
{
    return {
        separator(),
        {ui::action::RaiseCurrentItemAction, tr("Modifier")}
    };
}

JoystickButtonActionChoiceModel::JoystickButtonActionChoiceModel(bool withModifier, QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this, withModifier))
{
    d->formContent();
}

JoystickButtonActionChoiceModel::~JoystickButtonActionChoiceModel()
{
}

void JoystickButtonActionChoiceModel::initButtons(
    const QHash<QString, ui::action::IDType>& baseActions,
    const QHash<QString, ui::action::IDType>& modifiedActions)
{
    d->actionToButtons.clear();

    auto addButton = [this]
        (const QHash<QString, ui::action::IDType>& buttonsToActions, bool withModifier)
        {
            for (auto iter = buttonsToActions.begin(); iter != buttonsToActions.end(); ++iter)
            {
                if (iter.value() != ui::action::NoAction)
                    d->actionToButtons[iter.value()].insert({iter.key(), withModifier});
            }
        };

    addButton(baseActions, false);
    addButton(modifiedActions, true);
}

void JoystickButtonActionChoiceModel::setOpenLayoutChoiceEnabled(bool enabled)
{
    if (d->openLayoutChoiceEnabled == enabled)
        return;

    beginResetModel();
    d->openLayoutChoiceEnabled = enabled;
    d->formContent();
    endResetModel();
}

void JoystickButtonActionChoiceModel::setOpenLayoutChoiceVisible(bool visible)
{
    if (d->openLayoutChoiceVisible == visible)
        return;

    const int firstPos = itemsBeforeOpenLayoutItem().size();
    const int lastPos = firstPos + openLayoutItems().size() - 1;

    if (visible)
    {
        beginInsertRows(QModelIndex(), firstPos, lastPos);
        d->openLayoutChoiceVisible = visible;
        d->formContent();
        endInsertRows();
    }
    else
    {
        beginRemoveRows(QModelIndex(), firstPos, lastPos);
        d->openLayoutChoiceVisible = visible;
        d->formContent();
        endRemoveRows();
    }
}

int JoystickButtonActionChoiceModel::getRow(const QVariant& actionId) const
{
    const auto actionIdValue = actionId.value<ui::action::IDType>();
    for (int i = 0; i < d->info.size(); ++i)
    {
        const auto& actionInfo = d->info[i];
        if (actionInfo.id == actionId)
            return i;
    }

    return 0;
}

void JoystickButtonActionChoiceModel::setActionButton(
    ui::action::IDType actionId,
    const QString& buttonName,
    bool withModifier)
{
    auto buttonSearch = [this]
        (const QString& buttonName, bool withModifier)
        {
            for (auto iter = d->actionToButtons.begin(); iter != d->actionToButtons.end(); ++iter)
            {
                if (iter->contains({buttonName, withModifier}))
                    return iter;
            }

            return d->actionToButtons.end();
        };

    auto actionIter = buttonSearch(buttonName, withModifier);
    if (actionIter != d->actionToButtons.end())
        actionIter->remove({buttonName, withModifier});

    if (actionId != ui::action::NoAction)
        d->actionToButtons[actionId].insert({buttonName, withModifier});
}

QHash<int, QByteArray> JoystickButtonActionChoiceModel::roleNames() const
{
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles[ActionNameRole] = "name";
    roles[ActionIdRole] = "id";
    roles[ButtonsRole] = "buttons";
    roles[IsSeparatorRole] = "isSeparator";
    roles[IsEnabledRole] = "isEnabled";
    return roles;
}

QVariant JoystickButtonActionChoiceModel::data(const QModelIndex& index, int role) const
{
    if (!qBetween(0, index.row(), rowCount()) || index.column() != 0)
        return QVariant();

    const auto& actionInfo = d->info[index.row()];

    switch (role)
    {
        case ActionNameRole:
            return actionInfo.name;
        case ActionIdRole:
            return actionInfo.id;
        case ButtonsRole:
        {
            const auto buttons = d->actionToButtons[actionInfo.id];
            if (buttons.isEmpty())
                return QString();
            else if (actionInfo.id != kModifierActionId && buttons.size() > 1)
                return tr("Multiple");

            return buttons.begin()->withModifier
                ? nx::format("%1 + %2", kArrowsSymbol, buttons.begin()->name).toQString()
                : buttons.begin()->name;
        }
        case IsSeparatorRole:
            return actionInfo.isSeparator;
        case IsEnabledRole:
            return actionInfo.id != ui::action::OpenInNewTabAction || d->openLayoutChoiceEnabled;
    }

    return QVariant();
}

QModelIndex JoystickButtonActionChoiceModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    if (!qBetween(0, row, rowCount(parent)) || column != 0)
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex JoystickButtonActionChoiceModel::parent(const QModelIndex& index) const
{
    return QModelIndex();
}

int JoystickButtonActionChoiceModel::rowCount(const QModelIndex& parent) const
{
    return d->info.size();
}

} // namespace nx::vms::client::desktop::joystick
