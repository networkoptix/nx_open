// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>

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
#include <nx/vms/rules/action_builder_fields/target_user_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <ui/widgets/common/elided_label.h>
#include <ui/widgets/select_resources_button.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

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
    SourcePickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        m_selectResourceButton = createSelectButton();
        m_selectResourceButton->setSizePolicy(
            QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_selectResourceButton);

        {
            auto alertWidget = new QWidget;
            alertWidget->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

            auto alertLayout = new QHBoxLayout;
            alertLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            m_alertLabel = new QnElidedLabel;
            setWarningStyle(m_alertLabel);
            m_alertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            alertLayout->addWidget(m_alertLabel);

            alertWidget->setLayout(alertLayout);

            contentLayout->addWidget(alertWidget);
        }

        contentLayout->setStretch(0, 5);
        contentLayout->setStretch(1, 3);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QnSelectResourcesButton* m_selectResourceButton{};
    QnElidedLabel* m_alertLabel{};

    virtual void onFieldsSet() override
    {
        updateUi();

        connect(m_selectResourceButton,
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
        auto resourceList = resourcePool()->template getResourcesByIds<QnUserResource>(m_field->ids());

        auto selectUsersButtonPtr = static_cast<QnSelectUsersButton*>(m_selectResourceButton);
        if (m_field->acceptAll())
            selectUsersButtonPtr->selectAll();
        else
            selectUsersButtonPtr->selectUsers(resourceList);

        if (resourceList.isEmpty() && !m_field->acceptAll())
        {
            m_alertLabel->setText(SourcePickerWidgetStrings::selectUser());
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }

    void select()
    {
        auto selectedUsers = m_field->ids();

        ui::SubjectSelectionDialog dialog(this);
        dialog.setAllUsers(m_field->acceptAll());
        dialog.setCheckedSubjects(selectedUsers);
        if (dialog.exec() == QDialog::Rejected)
            return;

        m_field->setAcceptAll(dialog.allUsers());
        m_field->setIds(dialog.totalCheckedUsers());
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
    auto resourceList = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(m_field->ids());

    auto selectDeviceButtonPtr = static_cast<QnSelectDevicesButton*>(m_selectResourceButton);
    selectDeviceButtonPtr->selectDevices(resourceList);

    if (resourceList.isEmpty())
    {
        const auto allCameras = resourcePool()->getAllCameras();
        const auto deviceType
            = QnDeviceDependentStrings::calculateDeviceType(resourcePool(), allCameras);

        m_alertLabel->setText(SourcePickerWidgetStrings::selectDevice(deviceType));
        m_alertLabel->setVisible(true);
    }
    else
    {
        m_alertLabel->setVisible(false);
    }
}

template<>
void ServerPicker::updateUi()
{
    auto resourceList = resourcePool()->getResourcesByIds<QnMediaServerResource>(m_field->ids());

    auto selectServersButtonPtr = static_cast<QnSelectServersButton*>(m_selectResourceButton);
    selectServersButtonPtr->selectServers(resourceList);

    if (resourceList.isEmpty())
    {
        m_alertLabel->setText(SourcePickerWidgetStrings::selectServer());
        m_alertLabel->setVisible(true);
    }
    else
    {
        m_alertLabel->setVisible(false);
    }
}

template<>
void CameraPicker::select()
{
    auto selectedCameras = m_field->ids();

    CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
        selectedCameras,
        this);

    m_field->setIds(selectedCameras);
}

template<>
void ServerPicker::select()
{
    auto selectedServers = m_field->ids();

    const auto serverFilter = [](const QnMediaServerResourcePtr&){ return true; };
    ServerSelectionDialog::selectServers(selectedServers, serverFilter, QString{}, this);

    m_field->setIds(selectedServers);
}

} // namespace nx::vms::client::desktop::rules
