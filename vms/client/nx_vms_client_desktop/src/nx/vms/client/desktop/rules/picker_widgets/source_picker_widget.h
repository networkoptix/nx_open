// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>
#include <nx/vms/rules/event_fields/source_server_field.h>
#include <nx/vms/rules/event_fields/source_user_field.h>
#include <ui/widgets/common/elided_label.h>
#include <ui/widgets/select_resources_button.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types that could be represended as a list of ids. Implementation is dependent on the
 * field parameter.
 * Has implementation for:
 * - nx.events.fields.sourceCamera
 * - nx.events.fields.sourceServer
 * - nx.events.fields.sourceUser
 * - nx.actions.fields.targetUser
 */
template<typename F>
class SourcePickerWidget: public FieldPickerWidget<F>
{
public:
    explicit SourcePickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(label);

        {
            auto selectButtonLayout = new QHBoxLayout;
            selectButtonLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            selectResourceButton = createSelectButton();
            selectResourceButton->setSizePolicy(
                QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
            selectButtonLayout->addWidget(selectResourceButton);

            {
                auto alertWidget = new QWidget;
                alertWidget->setSizePolicy(
                    QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

                auto alertLayout = new QHBoxLayout;
                alertLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

                alertLabel = new QnElidedLabel;
                setWarningStyle(alertLabel);
                alertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                alertLayout->addWidget(alertLabel);

                alertWidget->setLayout(alertLayout);

                selectButtonLayout->addWidget(alertWidget);
            }

            selectButtonLayout->setStretch(0, 5);
            selectButtonLayout->setStretch(1, 3);

            mainLayout->addLayout(selectButtonLayout);
        }

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        selectResourceButton->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::tr;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QnSelectResourcesButton* selectResourceButton{};
    QnElidedLabel* alertLabel{};

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {
        updateUi();

        connect(selectResourceButton,
            &QPushButton::clicked,
            this,
            &SourcePickerWidget<F>::onSelectResourceButtonClicked,
            Qt::UniqueConnection);
    }

    void onSelectResourceButtonClicked()
    {
        select();
        updateUi();
    }

    QnSelectResourcesButton* createSelectButton()
    {
        return new QnSelectUsersButton;
    }

    void updateUi()
    {
        auto resourcePool = qnClientCoreModule->resourcePool();

        auto resourceList = resourcePool->getResourcesByIds<QnUserResource>(field->ids());

        auto selectUsersButtonPtr = static_cast<QnSelectUsersButton*>(selectResourceButton);
        if (field->acceptAll())
            selectUsersButtonPtr->selectAll();
        else
            selectUsersButtonPtr->selectUsers(resourceList);

        if (resourceList.isEmpty() && !field->acceptAll())
        {
            alertLabel->setText(tr("Select at least one User"));
            alertLabel->setVisible(true);
        }
        else
        {
            alertLabel->setVisible(false);
        }
    }

    void select()
    {
        auto selectedUsers = field->ids();

        ui::SubjectSelectionDialog dialog(this);
        dialog.setAllUsers(field->acceptAll());
        dialog.setCheckedSubjects(selectedUsers);
        if (dialog.exec() == QDialog::Rejected)
            return;

        field->setAcceptAll(dialog.allUsers());
        field->setIds(dialog.totalCheckedUsers());
    }
};

using CameraPicker = SourcePickerWidget<vms::rules::SourceCameraField>;
using ServerPicker = SourcePickerWidget<vms::rules::SourceServerField>;
using SourceUserPicker = SourcePickerWidget<vms::rules::SourceUserField>;
using TargetUserPicker = SourcePickerWidget<vms::rules::TargetUserField>;

template<>
QnSelectResourcesButton* CameraPicker::createSelectButton()
{
    return new QnSelectDevicesButton;
}

template<>
QnSelectResourcesButton* ServerPicker::createSelectButton()
{
    return new QnSelectServersButton;
}

template<>
void CameraPicker::updateUi()
{
    auto resourcePool = qnClientCoreModule->resourcePool();

    auto resourceList = resourcePool->getResourcesByIds<QnVirtualCameraResource>(field->ids());

    auto selectDeviceButtonPtr = static_cast<QnSelectDevicesButton*>(selectResourceButton);
    selectDeviceButtonPtr->selectDevices(resourceList);

    if (resourceList.isEmpty())
    {
        static const QnCameraDeviceStringSet deviceStringSet{
            tr("Select at least one Device"),
            tr("Select at least one Camera"),
            tr("Select at least one I/O module")
        };

        const auto allCameras = resourcePool->getAllCameras();
        const auto deviceType
            = QnDeviceDependentStrings::calculateDeviceType(resourcePool, allCameras);

        alertLabel->setText(deviceStringSet.getString(deviceType));
        alertLabel->setVisible(true);
    }
    else
    {
        alertLabel->setVisible(false);
    }
}

template<>
void ServerPicker::updateUi()
{
    auto resourcePool = qnClientCoreModule->resourcePool();

    auto resourceList = resourcePool->getResourcesByIds<QnMediaServerResource>(field->ids());

    auto selectServersButtonPtr = static_cast<QnSelectServersButton*>(selectResourceButton);
    selectServersButtonPtr->selectServers(resourceList);

    if (resourceList.isEmpty())
    {
        alertLabel->setText(tr("Select at least one Server"));
        alertLabel->setVisible(true);
    }
    else
    {
        alertLabel->setVisible(false);
    }
}

template<>
void CameraPicker::select()
{
    auto selectedCameras = field->ids();

    CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
        selectedCameras,
        this);

    field->setIds(selectedCameras);
}

template<>
void ServerPicker::select()
{
    auto selectedServers = field->ids();

    const auto serverFilter = [](const QnMediaServerResourcePtr&){ return true; };
    ServerSelectionDialog::selectServers(selectedServers, serverFilter, QString{}, this);

    field->setIds(selectedServers);
}

} // namespace nx::vms::client::desktop::rules
