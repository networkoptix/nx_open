// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_settings_dialog.h"

#include <QtQuick/QQuickItem>
#include <QtWidgets/QWidget>

#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/utils/qml_property.h>
#include <nx/vms/client/desktop/help/help_handler.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_dialogs/filtering/filtered_resource_proxy_model.h>
#include <nx/vms/client/desktop/resource_dialogs/models/resource_selection_decorator_model.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_views/models/resource_tree_icon_decorator_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/client_update_manager.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

#include "../settings/descriptors.h"
#include "../settings/device.h"
#include "../settings/manager.h"
#include "joystick_button_action_choice_model.h"
#include "joystick_button_settings_model.h"
#include "layout_intermediate_model.h"

namespace nx::vms::client::desktop::joystick {

using namespace entity_item_model;
using namespace entity_resource_tree;

using nx::vms::client::core::AccessController;

struct JoystickSettingsDialog::Private
{
    JoystickSettingsDialog* const q = nullptr;

    Manager* manager = nullptr;

    const std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> treeEntityBuilder;
    AbstractEntityPtr layoutsEntity;
    EntityItemModel layoutModel;
    LayoutIntermidiateModel layoutIntermidiateModel;
    ResourceTreeIconDecoratorModel iconDecoratorModel;
    ResourceSelectionDecoratorModel selectionDecoratorModel;
    FilteredResourceProxyModel filterModel;

    const std::unique_ptr<JoystickButtonSettingsModel> buttonSettingsModel;
    const std::unique_ptr<JoystickButtonActionChoiceModel> buttonBaseActionChoiceModel;
    const std::unique_ptr<JoystickButtonActionChoiceModel> buttonModifiedActionChoiceModel;

    Private(JoystickSettingsDialog* owner, Manager* manager);
    virtual ~Private();

    bool initModel(bool initWithDefaults = false);

    QString getButtonName(int row) const;
    ui::action::IDType getActionId(int row, int column) const;

    nx::utils::ScopedConnections connections;

    nx::utils::ScopedConnection deviceConnection;
};

JoystickSettingsDialog::Private::Private(JoystickSettingsDialog* owner, Manager* manager):
    q(owner),
    manager(manager),
    treeEntityBuilder(new ResourceTreeEntityBuilder(q->systemContext())),
    buttonSettingsModel(new JoystickButtonSettingsModel(&filterModel, owner)),
    buttonBaseActionChoiceModel(new JoystickButtonActionChoiceModel(true, owner)),
    buttonModifiedActionChoiceModel(new JoystickButtonActionChoiceModel(false, owner))
{
    treeEntityBuilder->setUser(q->systemContext()->accessController()->user());
    layoutsEntity = treeEntityBuilder->createDialogAllLayoutsEntity();
    layoutModel.setRootEntity(layoutsEntity.get());
    iconDecoratorModel.setSourceModel(&layoutModel);
    selectionDecoratorModel.setSourceModel(&iconDecoratorModel);
    layoutIntermidiateModel.setSourceModel(&selectionDecoratorModel);
    filterModel.setSourceModel(&layoutIntermidiateModel);

    QObject::connect(buttonSettingsModel.get(), &JoystickButtonSettingsModel::modelReset,
        [this]
        {
            QHash<QString, ui::action::IDType> baseActionButtons;
            for (int row = 0; row < buttonSettingsModel->rowCount(); ++row)
            {
                const QString buttonName = getButtonName(row);
                const ui::action::IDType actionId =
                    getActionId(row, JoystickButtonSettingsModel::BaseActionColumn);

                baseActionButtons[buttonName] = actionId;
            }

            QHash<QString, ui::action::IDType> modifiedActionButtons;
            for (int row = 0; row < buttonSettingsModel->rowCount(); ++row)
            {
                const QString buttonName = getButtonName(row);
                const ui::action::IDType actionId =
                    getActionId(row, JoystickButtonSettingsModel::ModifiedActionColumn);

                modifiedActionButtons[buttonName] = actionId;
            }

            buttonBaseActionChoiceModel->initButtons(baseActionButtons, modifiedActionButtons);
            buttonModifiedActionChoiceModel->initButtons(baseActionButtons, modifiedActionButtons);
        });

    QObject::connect(buttonSettingsModel.get(), &JoystickButtonSettingsModel::dataChanged,
        [this](const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QVector<int>& roles)
        {
            if (roles != QVector<int>{JoystickButtonSettingsModel::ButtonPressedRole})
                QmlProperty<bool>(q->rootObjectHolder(), "applyButtonEnabled") = true;

            if (!roles.contains(JoystickButtonSettingsModel::ActionIdRole))
                return;

            for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
            {
                if (topLeft.column() <= JoystickButtonSettingsModel::BaseActionColumn
                    && bottomRight.column() >= JoystickButtonSettingsModel::BaseActionColumn)
                {
                    const QString buttonName = getButtonName(row);
                    const ui::action::IDType actionId =
                        getActionId(row, JoystickButtonSettingsModel::BaseActionColumn);

                    buttonBaseActionChoiceModel->setActionButton(actionId, buttonName);
                    buttonModifiedActionChoiceModel->setActionButton(actionId, buttonName);
                }

                if (topLeft.column() <= JoystickButtonSettingsModel::ModifiedActionColumn
                    && bottomRight.column() >= JoystickButtonSettingsModel::ModifiedActionColumn)
                {
                    const QString buttonName = getButtonName(row);
                    const ui::action::IDType actionId =
                        getActionId(row, JoystickButtonSettingsModel::ModifiedActionColumn);

                    buttonBaseActionChoiceModel->setActionButton(actionId, buttonName, true);
                    buttonModifiedActionChoiceModel->setActionButton(actionId, buttonName, true);
                }
            }
        });

    QObject::connect(
        buttonSettingsModel.get(),
        &JoystickButtonSettingsModel::panAndTiltSensitivityChanged,
        [this](bool initialization)
        {
            if (!initialization)
                QmlProperty<bool>(q->rootObjectHolder(), "applyButtonEnabled") = true;
        });

    QObject::connect(
        buttonSettingsModel.get(),
        &JoystickButtonSettingsModel::zoomSensitivityChanged,
        [this](bool initialization)
        {
            if (!initialization)
                QmlProperty<bool>(q->rootObjectHolder(), "applyButtonEnabled") = true;
        });
}

JoystickSettingsDialog::Private::~Private()
{
    layoutModel.setRootEntity(nullptr);
}

bool JoystickSettingsDialog::Private::initModel(bool initWithDefaults)
{
    const QList<const Device*> devices = manager->devices();
    if (devices.empty())
        return false;

    const auto& device = devices.first();
    const JoystickDescriptor& description = initWithDefaults
        ? manager->getDefaultDeviceDescription(device->modelName())
        : manager->getDeviceDescription(device->modelName());
    buttonSettingsModel->init(description, device);

    const auto stickPosition = device->currentStickPosition();
    QmlProperty<bool>(q->rootObjectHolder(), "panAndTiltHighlighted") =
        stickPosition[Device::xIndex] != 0 || stickPosition[Device::yIndex] != 0;
    QmlProperty<bool>(q->rootObjectHolder(), "zoomHighlighted") =
        stickPosition[Device::zIndex] != 0;

    deviceConnection.reset(QObject::connect(device, &Device::stateChanged, q,
        [this](const Device::StickPosition& stick)
        {
            QmlProperty<bool>(q->rootObjectHolder(), "panAndTiltHighlighted")
                = stick[Device::xIndex] != 0 || stick[Device::yIndex] != 0;

            QmlProperty<bool>(q->rootObjectHolder(), "zoomHighlighted")
                = stick[Device::zIndex] != 0;
        }));

    const bool openLayoutChoiceEnabled = !q->currentServer().isNull();
    const bool openLayoutChoiceVisible =
        !q->currentServer().isNull() || buttonSettingsModel->openLayoutActionIsSet();

    buttonBaseActionChoiceModel->setOpenLayoutChoiceEnabled(openLayoutChoiceEnabled);
    buttonBaseActionChoiceModel->setOpenLayoutChoiceVisible(openLayoutChoiceVisible);

    buttonModifiedActionChoiceModel->setOpenLayoutChoiceEnabled(openLayoutChoiceEnabled);
    buttonModifiedActionChoiceModel->setOpenLayoutChoiceVisible(openLayoutChoiceVisible);

    return true;
}

QString JoystickSettingsDialog::Private::getButtonName(int row) const
{
    const QModelIndex buttonNameIndex = buttonSettingsModel->index(
        row, JoystickButtonSettingsModel::ButtonNameColumn);
    return buttonSettingsModel->data(buttonNameIndex, Qt::DisplayRole).toString();
};

ui::action::IDType JoystickSettingsDialog::Private::getActionId(int row, int column) const
{
    const QModelIndex actionIndex = buttonSettingsModel->index(row, column);
    return (ui::action::IDType)buttonSettingsModel->data(
        actionIndex, JoystickButtonSettingsModel::ActionIdRole).toInt();
};

JoystickSettingsDialog::JoystickSettingsDialog(Manager* manager, QWidget* parent):
    base_type(
        qnClientCoreModule->mainQmlEngine(),
        QUrl("Nx/Dialogs/JoystickSettings/JoystickSettingsDialog.qml"),
        {},
        parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this, manager))
{
    QmlProperty<bool>(rootObjectHolder(), "joystickIsSupported") = d->initModel();

    QmlProperty<QString>(rootObjectHolder(), "joystickName") =
        d->buttonSettingsModel->getDescriptionState().model;

    QmlProperty<QObject*>(rootObjectHolder(), "buttonSettingsModel") =
        d->buttonSettingsModel.get();
    QmlProperty<QObject*>(rootObjectHolder(), "buttonBaseActionChoiceModel") =
        d->buttonBaseActionChoiceModel.get();
    QmlProperty<QObject*>(rootObjectHolder(), "buttonModifiedActionChoiceModel") =
        d->buttonModifiedActionChoiceModel.get();

    QmlProperty<QObject*>(rootObjectHolder(), "layoutModel") = &d->filterModel;

    QmlProperty<bool>(rootObjectHolder(), "connectedToServer") = !context()->user().isNull();

    d->connections << connect(context(),
        &QnWorkbenchContext::userChanged,
        this,
        [this](const QnUserResourcePtr& user)
        {
            const bool connectedToServer = !user.isNull();

            QmlProperty<bool>(rootObjectHolder(), "connectedToServer") = connectedToServer;

            const bool openLayoutChoiceEnabled = connectedToServer;
            const bool openLayoutChoiceVisible =
                connectedToServer || d->buttonSettingsModel->openLayoutActionIsSet();

            d->buttonBaseActionChoiceModel->setOpenLayoutChoiceEnabled(openLayoutChoiceEnabled);
            d->buttonBaseActionChoiceModel->setOpenLayoutChoiceVisible(openLayoutChoiceVisible);

            d->buttonModifiedActionChoiceModel->setOpenLayoutChoiceEnabled(
                openLayoutChoiceEnabled);
            d->buttonModifiedActionChoiceModel->setOpenLayoutChoiceVisible(
                openLayoutChoiceVisible);
        });

    d->connections << connect(systemContext()->accessController(), &AccessController::userChanged,
        [this]
        {
            d->treeEntityBuilder->setUser(systemContext()->accessController()->user());
            auto layoutsEntity = d->treeEntityBuilder->createDialogAllLayoutsEntity();
            d->layoutModel.setRootEntity(layoutsEntity.get());
            d->layoutsEntity = std::move(layoutsEntity);
        });

    auto handleApply =
        [this, manager]
        {
            manager->updateDeviceDescription(d->buttonSettingsModel->getDescriptionState());
            manager->saveConfig(d->buttonSettingsModel->getDescriptionState().model);
            QmlProperty<bool>(rootObjectHolder(), "applyButtonEnabled") = false;
        };

    auto handleReset =
        [this]
        {
            d->initModel(true);
            QmlProperty<bool>(rootObjectHolder(), "applyButtonEnabled") = true;
        };

    connect(this, &JoystickSettingsDialog::accepted, handleApply);
    connect(this, &JoystickSettingsDialog::applied, handleApply);

    connect(rootObjectHolder()->object(), SIGNAL(resetToDefault()),
        this, SIGNAL(resetToDefault()));
    connect(this, &JoystickSettingsDialog::resetToDefault, handleReset);

    setHelpTopic(this, HelpTopic::Id::UsingJoystick);
}

JoystickSettingsDialog::~JoystickSettingsDialog()
{
}

void JoystickSettingsDialog::initWithCurrentActiveJoystick()
{
    d->initModel(false);
}

} // namespace nx::vms::client::desktop::joystick
