// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_button_settings_model.h"

#include <QtGui/QAction>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../settings/action_factory.h"
#include "../settings/descriptors.h"
#include "layout_intermediate_model.h"

namespace nx::vms::client::desktop::joystick {

namespace {

const int kMaxActionsNumber = 2;

QString offlineLayoutSelectedText()
{
    return JoystickButtonSettingsModel::tr("Selected Layout");
}

QString noLayoutSelectedText()
{
    return JoystickButtonSettingsModel::tr("Select Layout...");
}

int getLayoutLogicalId(const ButtonDescriptor& buttonDescription, int actionIndex)
{
    if (buttonDescription.actions.size() <= actionIndex)
        return 0;

    const auto& parametersList =
        buttonDescription.actions[actionIndex].parameters;

    const auto paramIter = parametersList.find(ActionFactory::kLayoutLogicalIdParameterName);
    if (paramIter == parametersList.end())
        return 0;

    bool ok = false;
    const int layoutLogicalId = paramIter->second.toInt(&ok);

    return ok ? layoutLogicalId : 0;
}

QString getLayoutUuid(const ButtonDescriptor& buttonDescription, int actionIndex)
{
    if (buttonDescription.actions.size() <= actionIndex)
        return QString();

    const auto& parametersList =
        buttonDescription.actions[actionIndex].parameters;

    const auto paramIter = parametersList.find(ActionFactory::kLayoutUuidParameterName);
    if (paramIter == parametersList.end())
        return QString();

    return QnUuid(paramIter->second).toString();
}

QString formButtonName(const QString& rawButtonName)
{
    return nx::format("%1 %2", JoystickButtonSettingsModel::tr("Button"), rawButtonName);
}

QString getModifierButtonName(const JoystickDescriptor& description, const Device* device)
{
    const int visibleButtonsNumber =
        std::min(description.buttons.size(), device->initializedButtonsNumber());

    for (int i = 0; i < visibleButtonsNumber; ++i)
    {
        const auto& buttonDescription = description.buttons[i];
        for (const auto& actionDescription: buttonDescription.actions)
        {
            if (actionDescription.id == kModifierActionId)
                return formButtonName(buttonDescription.name);
        }
    }

    return QString();
}

int getHotkeyIndex(const ButtonDescriptor& buttonDescription, int actionIndex)
{
    if (buttonDescription.actions.size() <= actionIndex)
        return 0;

    const auto& parametersList =
        buttonDescription.actions[actionIndex].parameters;

    const auto paramIter = parametersList.find(ActionFactory::kHotkeyParameterName);
    if (paramIter == parametersList.end())
        return 0;

    bool ok = false;
    const int hotkeyIndex = paramIter->second.toInt(&ok);
    if (!ok)
        return 0;

    return hotkeyIndex;
}

void expandActionsArray(ButtonDescriptor* buttonDescription, int actionsNumber)
{
    if (buttonDescription->actions.size() <= actionsNumber)
    {
        while (buttonDescription->actions.size() < actionsNumber)
            buttonDescription->actions.push_back({});
    }
}

QVector<int> setOpenLayoutParameter(
    ActionDescriptor* actionDescription,
    int role,
    const QVariant& value)
{
    actionDescription->parameters.clear();
    switch (role)
    {
        case JoystickButtonSettingsModel::LayoutLogicalIdRole:
        {
            actionDescription->parameters[ActionFactory::kLayoutLogicalIdParameterName] =
                value.toString();
            return {Qn::ResourceIconKeyRole, JoystickButtonSettingsModel::LayoutLogicalIdRole};
        }
        case JoystickButtonSettingsModel::LayoutUuidRole:
        {
            actionDescription->parameters[ActionFactory::kLayoutUuidParameterName] =
                value.toString();
            return {Qn::ResourceIconKeyRole, JoystickButtonSettingsModel::LayoutLogicalIdRole};
        }
    }

    NX_ASSERT(false, nx::format("Unexpected setOpenLayoutParameter role: %1", role));
    return {};
}

} // namespace

struct JoystickButtonSettingsModel::Private
{
    JoystickButtonSettingsModel* const q;

    FilteredResourceProxyModel* const resourceModel = nullptr;
    QnResourceIconCache* const resourceIconCache = nullptr;

    JoystickDescriptor description;

    QString modifierButtonName;
    bool zAxisIsInitialized = false;
    qsizetype initializedButtonsNumber = 0;

    bool connectedToServer = false;

    QVector<bool> buttonPressedStates;

    nx::utils::ScopedConnections deviceConnections;

private:
    nx::utils::ScopedConnections scopedConnections;

public:
    Private(JoystickButtonSettingsModel* parent, FilteredResourceProxyModel* resourceModel):
        q(parent),
        resourceModel(resourceModel),
        resourceIconCache(qnResIconCache),
        connectedToServer(q->system()->user())
    {
        scopedConnections.add(connect(q->system(),
            &SystemContext::userChanged,
            [this](const QnUserResourcePtr& user)
            {
                connectedToServer = !user.isNull();

                for (int row = 0; row < q->rowCount(); ++row)
                {
                    const menu::IDType baseActionId = qvariant_cast<menu::IDType>(
                        q->data(q->index(row, BaseActionColumn), ActionIdRole));
                    if (baseActionId == menu::OpenInNewTabAction)
                    {
                        emit q->dataChanged(
                            q->index(row, BaseActionColumn),
                            q->index(row, BaseActionParameterColumn),
                            {IsEnabledRole, Qt::DisplayRole});
                    }

                    const menu::IDType modifiedActionId = qvariant_cast<menu::IDType>(
                        q->data(q->index(row, ModifiedActionColumn), ActionIdRole));
                    if (modifiedActionId == menu::OpenInNewTabAction)
                    {
                        emit q->dataChanged(
                            q->index(row, ModifiedActionColumn),
                            q->index(row, ModifiedActionParameterColumn),
                            {IsEnabledRole, Qt::DisplayRole});
                    }
                }
            }));

        scopedConnections.add(connect(resourceModel, &FilteredResourceProxyModel::modelReset,
            [this]
            {
                for (int row = 0; row < q->rowCount(); ++row)
                {
                    const menu::IDType baseActionId = qvariant_cast<menu::IDType>(
                        q->data(q->index(row, BaseActionColumn), ActionIdRole));
                    if (baseActionId == menu::OpenInNewTabAction)
                    {
                        emit q->dataChanged(
                            q->index(row, BaseActionParameterColumn),
                            q->index(row, BaseActionParameterColumn),
                            {Qt::DisplayRole, LayoutLogicalIdRole, LayoutUuidRole});
                    }

                    const menu::IDType modifiedActionId = qvariant_cast<menu::IDType>(
                        q->data(q->index(row, ModifiedActionColumn), ActionIdRole));
                    if (modifiedActionId == menu::OpenInNewTabAction)
                    {
                        emit q->dataChanged(
                            q->index(row, ModifiedActionParameterColumn),
                            q->index(row, ModifiedActionParameterColumn),
                            {Qt::DisplayRole, LayoutLogicalIdRole, LayoutUuidRole});
                    }
                }
            }));
    }

    ~Private()
    {
    }

    void initDeviceData(const Device* device);
    void overwriteAction(const QModelIndex& index, menu::IDType newActionId);
    void resetPrevModifierButtonAction();
    void setModifierAction(const QModelIndex& index, ButtonDescriptor* buttonDescription);
    void setAnotherAction(
        const QModelIndex& index,
        ActionDescriptor* actionDescription,
        menu::IDType newActionId);

    QModelIndex getResourceByLogicalId(int logicalId) const;
    QModelIndex getResourceByUuid(const QString& uuid) const;

    void setModifierButtonName(const QString& name);
};

void JoystickButtonSettingsModel::Private::initDeviceData(const Device* device)
{
    zAxisIsInitialized = device->zAxisIsInitialized();
    initializedButtonsNumber = device->initializedButtonsNumber();

    buttonPressedStates.fill(false, q->rowCount());

    deviceConnections.reset();

    deviceConnections << connect(device, &Device::buttonPressed, q,
        [this](int buttonIndex)
        {
            if (!NX_ASSERT(buttonIndex < q->rowCount()
                && buttonIndex < buttonPressedStates.size()))
            {
                return;
            }

            buttonPressedStates[buttonIndex] = true;

            const QModelIndex index = q->index(buttonIndex, 0);
            emit q->dataChanged(index, index, {ButtonPressedRole});
        });

    deviceConnections << connect(device, &Device::buttonReleased, q,
        [this](int buttonIndex)
        {
            if (!NX_ASSERT(buttonIndex < q->rowCount()
                && buttonIndex < buttonPressedStates.size()))
            {
                return;
            }

            buttonPressedStates[buttonIndex] = false;

            const QModelIndex index = q->index(buttonIndex, 0);
            emit q->dataChanged(index, index, {ButtonPressedRole});
        });

    deviceConnections << connect(device, &Device::axisIsInitializedChanged, q,
        [this, device]
        {
            zAxisIsInitialized = device->zAxisIsInitialized();
            q->zoomIsEnabledChanged();
        });

    deviceConnections << connect(device, &Device::initializedButtonsNumberChanged, q,
        [this, device]
        {
            // It is unlikely situation, therefore this inafficiency is permissible.
            q->beginResetModel();
            initializedButtonsNumber = device->initializedButtonsNumber();
            getModifierButtonName(description, device);
            q->endResetModel();
        });
}

void JoystickButtonSettingsModel::Private::overwriteAction(
    const QModelIndex& index,
    menu::IDType newActionId)
{
    const int actionListIndex = (index.column() == BaseActionColumn) ? 0 : 1;

    auto& buttonDescription = description.buttons[index.row()];

    expandActionsArray(&buttonDescription, actionListIndex + 1);

    auto& actionDescription = buttonDescription.actions[actionListIndex];

    if (newActionId == actionDescription.id)
        return;

    if (newActionId == kModifierActionId || actionDescription.id == kModifierActionId)
        resetPrevModifierButtonAction();

    if (newActionId == kModifierActionId)
        setModifierAction(index, &buttonDescription);
    else
        setAnotherAction(index, &actionDescription, newActionId);
}

void JoystickButtonSettingsModel::Private::resetPrevModifierButtonAction()
{
    for (int i = 0; i < description.buttons.size(); ++i)
    {
        auto& prevButtonDescription = description.buttons[i];
        if (!prevButtonDescription.actions.isEmpty()
            && prevButtonDescription.actions[0].id == kModifierActionId)
        {
            for (auto& actionDescription: prevButtonDescription.actions)
                actionDescription.id = menu::NoAction;

            emit q->dataChanged(
                q->index(i, FirstColumn),
                q->index(i, LastColumn),
                {ActionIdRole, IsModifierRole, IsEnabledRole});

            setModifierButtonName("");
        }
    }
}

void JoystickButtonSettingsModel::Private::setModifierAction(
    const QModelIndex& index,
    ButtonDescriptor* buttonDescription)
{
    expandActionsArray(buttonDescription, kMaxActionsNumber);

    for (auto& actionDescription: buttonDescription->actions)
        actionDescription.id = kModifierActionId;

    emit q->dataChanged(
        q->index(index.row(), FirstColumn),
        q->index(index.row(), LastColumn),
        {IsModifierRole, IsEnabledRole, ActionParameterTypeRole, ActionIdRole, Qt::DisplayRole});

    setModifierButtonName(formButtonName(buttonDescription->name));
}

void JoystickButtonSettingsModel::Private::setAnotherAction(
    const QModelIndex& index,
    ActionDescriptor* actionDescription,
    menu::IDType newActionId)
{
    actionDescription->id = newActionId;

    actionDescription->parameters.clear();
    if (actionDescription->id == menu::PtzActivateByHotkeyAction)
        actionDescription->parameters[ActionFactory::kHotkeyParameterName] = "0";
    else if (actionDescription->id == menu::OpenInNewTabAction)
        actionDescription->parameters[ActionFactory::kLayoutLogicalIdParameterName] = "0";

    emit q->dataChanged(
        index,
        q->index(index.row(), index.column() + 1),
        {ActionParameterTypeRole, ActionIdRole, Qt::DisplayRole});
}

QModelIndex JoystickButtonSettingsModel::Private::getResourceByLogicalId(int logicalId) const
{
    NX_ASSERT(resourceModel);

    if (logicalId == 0)
        return QModelIndex();

    for (int col = 0; col < resourceModel->rowCount(); ++col)
    {
        const QModelIndex index = resourceModel->index(col, 0);
        const auto otherLogicalId = resourceModel->data(
            index, LayoutIntermidiateModel::LogicalIdRole).toInt();

        if (otherLogicalId == logicalId)
            return index;
    }

    return QModelIndex();
}

QModelIndex JoystickButtonSettingsModel::Private::getResourceByUuid(const QString& uuid) const
{
    NX_ASSERT(resourceModel);

    if (uuid.isNull())
        return QModelIndex();

    for (int col = 0; col < resourceModel->rowCount(); ++col)
    {
        const QModelIndex index = resourceModel->index(col, 0);
        const auto otherUuid = resourceModel->data(index, Qn::ItemUuidRole).toString();

        if (otherUuid == uuid)
            return index;
    }

    return QModelIndex();
}

void JoystickButtonSettingsModel::Private::setModifierButtonName(const QString& name)
{
    modifierButtonName = name;
    emit q->modifierButtonNameChanged();
}

JoystickButtonSettingsModel::JoystickButtonSettingsModel(
    WindowContext* windowContext,
    FilteredResourceProxyModel* resourceModel,
    QObject* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    d(new Private(this, resourceModel))
{
    NX_ASSERT(resourceModel);
}

JoystickButtonSettingsModel::~JoystickButtonSettingsModel()
{
}

void JoystickButtonSettingsModel::init(const JoystickDescriptor& description, const Device* device)
{
    beginResetModel();
    d->description = description;
    endResetModel();

    d->initDeviceData(device);

    d->setModifierButtonName(getModifierButtonName(description, device));

    emit panAndTiltSensitivityChanged(true);
    emit zoomSensitivityChanged(true);

    emit zoomIsEnabledChanged();
}

JoystickDescriptor JoystickButtonSettingsModel::getDescriptionState() const
{
    return d->description;
}

QString JoystickButtonSettingsModel::modifierButtonName() const
{
    return d->modifierButtonName;
}

bool JoystickButtonSettingsModel::zoomIsEnabled() const
{
    return d->zAxisIsInitialized;
}

double JoystickButtonSettingsModel::panAndTiltSensitivity() const
{
    return d->description.xAxis.sensitivity.toDouble();
}

void JoystickButtonSettingsModel::setPanAndTiltSensitivity(double value)
{
    const auto panAndTiltSensitivity = QString::number(value);
    d->description.xAxis.sensitivity = panAndTiltSensitivity;
    d->description.yAxis.sensitivity = panAndTiltSensitivity;
    emit panAndTiltSensitivityChanged();
}

double JoystickButtonSettingsModel::zoomSensitivity() const
{
    return d->description.zAxis.sensitivity.toDouble();
}

void JoystickButtonSettingsModel::setZoomSensitivity(double value)
{
    d->description.zAxis.sensitivity = QString::number(value);
    emit zoomSensitivityChanged();
}

bool JoystickButtonSettingsModel::openLayoutActionIsSet() const
{
    return std::any_of(d->description.buttons.begin(), d->description.buttons.end(),
        [](const ButtonDescriptor& buttonDescription)
        {
            if (buttonDescription.actions.isEmpty())
                return false;

            if (buttonDescription.actions[0].id == menu::OpenInNewTabAction)
                return true;

            if (buttonDescription.actions.size() < 2)
                return false;

            if (buttonDescription.actions[1].id == menu::OpenInNewTabAction)
                return true;

            return false;
        });
}

QHash<int, QByteArray> JoystickButtonSettingsModel::roleNames() const
{
    QHash<int, QByteArray> roles = base_type::roleNames();

    roles[ActionParameterTypeRole] = "actionParameterType";
    roles[ActionIdRole] = "actionId";
    roles[HotkeyIndexRole] = "hotkeyIndex";
    roles[LayoutLogicalIdRole] = "layoutLogicalId";
    roles[LayoutUuidRole] = "layoutUuid";
    roles[IsModifierRole] = "isModifier";
    roles[IsEnabledRole] = "isEnabled";
    roles[ButtonPressedRole] = "buttonPressed";

    roles[Qn::ResourceIconKeyRole] = "iconKey";

    return roles;
}

QVariant JoystickButtonSettingsModel::data(const QModelIndex& index, int role) const
{
    const bool indexIsValid = index.isValid()
        && qBetween(0, index.row(), rowCount())
        && qBetween(0, index.column(), columnCount());
    if (!NX_ASSERT(indexIsValid))
        return QVariant();

    const bool isParameterColumn = index.column() == BaseActionParameterColumn
        || index.column() == ModifiedActionParameterColumn;

    const auto& buttonDescription = d->description.buttons[index.row()];

    const int actionIndex = index.column() >= ModifiedActionColumn ? 1 : 0;

    const menu::IDType actionId = buttonDescription.actions.size() > actionIndex
        ? buttonDescription.actions[actionIndex].id
        : menu::NoAction;

    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (index.column() == ButtonNameColumn)
                return formButtonName(buttonDescription.name);
            else if ((index.column() == BaseActionParameterColumn
                || index.column() == ModifiedActionParameterColumn)
                && actionId == menu::OpenInNewTabAction)
            {
                if (!d->connectedToServer)
                    return offlineLayoutSelectedText();

                const auto layoutLogicalId = getLayoutLogicalId(buttonDescription, actionIndex);
                const auto layoutUuid = getLayoutUuid(buttonDescription, actionIndex);

                QModelIndex resourceIndex;

                if (layoutLogicalId != 0)
                    resourceIndex = d->getResourceByLogicalId(layoutLogicalId);
                else if (!layoutUuid.isEmpty())
                    resourceIndex = d->getResourceByUuid(layoutUuid);

                if (resourceIndex.isValid())
                {
                    const QVariant layoutName = d->resourceModel->data(resourceIndex);

                    if (layoutName.isValid())
                        return layoutName;
                }

                return noLayoutSelectedText();
            }

            break;
        }
        case Qn::ResourceIconKeyRole:
        {
            if (index.column() == BaseActionParameterColumn
                || index.column() == ModifiedActionParameterColumn)
            {
                if (actionId != menu::OpenInNewTabAction)
                    return QVariant();

                const auto layoutLogicalId = getLayoutLogicalId(buttonDescription, actionIndex);

                if (layoutLogicalId == 0)
                    return QVariant();

                const QModelIndex resourceIndex = d->getResourceByLogicalId(layoutLogicalId);
                if (resourceIndex.isValid())
                    return d->resourceModel->data(resourceIndex, Qn::ResourceIconKeyRole);

                return QVariant();
            }

            break;
        }
        case ActionParameterTypeRole:
        {
            if (!isParameterColumn)
                return NoParameter;

            switch (actionId)
            {
                case menu::PtzActivateByHotkeyAction:
                    return HotkeyParameter;
                case menu::OpenInNewTabAction:
                    return LayoutParameter;
                default:
                    return NoParameter;
            }
        }
        case ActionIdRole:
            return actionId;
        case HotkeyIndexRole:
            return getHotkeyIndex(buttonDescription, actionIndex);
        case IsModifierRole:
            return buttonDescription.actions[0].id == kModifierActionId;
        case IsEnabledRole:
        {
            if (index.column() >= ModifiedActionColumn)
            {
                const menu::IDType firstActionId = !buttonDescription.actions.isEmpty()
                    ? buttonDescription.actions[0].id
                    : menu::NoAction;

                if (firstActionId == kModifierActionId)
                    return false;
            }

            if (d->connectedToServer)
                return true;

            if (actionId == menu::OpenInNewTabAction
                && (index.column() == BaseActionParameterColumn
                || index.column() == ModifiedActionParameterColumn))
            {
                return false;
            }

            return true;
        }
        case LayoutLogicalIdRole:
        {
            if ((index.column() == BaseActionParameterColumn
                || index.column() == ModifiedActionParameterColumn)
                && actionId == menu::OpenInNewTabAction)
            {
                return getLayoutLogicalId(buttonDescription, actionIndex);
            }

            break;
        }
        case LayoutUuidRole:
        {
            if ((index.column() == BaseActionParameterColumn
                || index.column() == ModifiedActionParameterColumn)
                && actionId == menu::OpenInNewTabAction)
            {
                const auto layoutUuid = getLayoutUuid(buttonDescription, actionIndex);

                return !layoutUuid.isEmpty() ? layoutUuid : QVariant();
            }

            break;
        }
        case ButtonPressedRole:
        {
            if (index.column() == ButtonNameColumn)
                return d->buttonPressedStates[index.row()];

            break;
        }
    }

    NX_ASSERT(false, nx::format("Unexpected data request. Index row: %1, col: %2. actionId: %3",
        index.row(), index.column(), actionId));

    return QVariant();
}

bool JoystickButtonSettingsModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    const bool indexIsValid = index.isValid()
        && qBetween(0, index.row(), rowCount())
        && qBetween(0, index.column(), columnCount());
    if (!NX_ASSERT(indexIsValid))
        return false;

    switch (role)
    {
        case ActionIdRole:
        {
            if (index.column() == BaseActionColumn || index.column() == ModifiedActionColumn)
            {
                d->overwriteAction(index, (menu::IDType) value.toInt());

                emit dataChanged(
                    index,
                    this->index(index.row(), index.column() + 1),
                    {IsEnabledRole});

                return true;
            }

            break;
        }
        case HotkeyIndexRole:
        case LayoutLogicalIdRole:
        case LayoutUuidRole:
        {
             if (index.column() == BaseActionParameterColumn
                 || index.column() == ModifiedActionParameterColumn)
             {
                 int actionListIndex = index.column() == BaseActionParameterColumn ? 0 : 1;

                 auto& buttonDescription = d->description.buttons[index.row()];

                 if (buttonDescription.actions.size() <= actionListIndex)
                 {
                     NX_ASSERT(false, "Invalid model state.");
                     return false;
                 }

                 auto& actionDescription = buttonDescription.actions[actionListIndex];

                 QVector<int> roles = {Qt::DisplayRole};

                 switch (actionDescription.id)
                 {
                     case menu::PtzActivateByHotkeyAction:
                         actionDescription.parameters["hotkey"] = value.toString();
                         break;
                     case menu::OpenInNewTabAction:
                         roles += setOpenLayoutParameter(&actionDescription, role, value);
                         break;
                     default:
                     {
                         NX_ASSERT(false, "Unexpected action type.");
                         return false;
                     }
                 }

                 emit dataChanged(index, index, roles);

                 return true;
             }

             break;
        }
    }

    NX_ASSERT(false, nx::format("Unexpected setData call. Index row: %1, col: %2.",
        index.row(), index.column()));

    return false;
}

QModelIndex JoystickButtonSettingsModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    if (!qBetween(0, row, rowCount(parent)) || !qBetween(0, column, columnCount(parent)))
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex JoystickButtonSettingsModel::parent(const QModelIndex& /*index*/) const
{
    return QModelIndex();
}

int JoystickButtonSettingsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return std::min(d->description.buttons.size(), d->initializedButtonsNumber);
}

int JoystickButtonSettingsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return LastColumn + 1;
}

} // namespace nx::vms::client::desktop::joystick
