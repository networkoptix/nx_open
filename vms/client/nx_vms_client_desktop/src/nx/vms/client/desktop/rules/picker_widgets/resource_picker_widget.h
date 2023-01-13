// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

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
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
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
 * - nx.actions.fields.devices
 */
template<typename F, typename B>
class ResourcePickerWidgetBase: public FieldPickerWidget<F>
{
public:
    explicit ResourcePickerWidgetBase(SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QVBoxLayout;
        contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        auto selectResourceLayout = new QHBoxLayout;

        m_selectButton = new B;
        m_selectButton->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        selectResourceLayout->addWidget(m_selectButton);

        {
            auto alertWidget = new QWidget;
            alertWidget->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

            auto alertLayout = new QHBoxLayout;
            alertLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            m_alertLabel = new QnElidedLabel;
            setWarningStyle(m_alertLabel);
            m_alertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_alertLabel->setElideMode(Qt::ElideRight);
            alertLayout->addWidget(m_alertLabel);

            alertWidget->setLayout(alertLayout);

            selectResourceLayout->addWidget(alertWidget);
        }

        selectResourceLayout->setStretch(0, 5);
        selectResourceLayout->setStretch(1, 3);

        contentLayout->addLayout(selectResourceLayout);

        m_contentWidget->setLayout(contentLayout);
    }

protected:
    PICKER_WIDGET_COMMON_USINGS

    B* m_selectButton{};
    QnElidedLabel* m_alertLabel{};

    virtual void onSelectButtonClicked() = 0;
    virtual void updateUi() = 0;

    void onFieldsSet() override
    {
        updateUi();

        connect(m_selectButton,
            &B::clicked,
            this,
            &ResourcePickerWidgetBase<F, B>::onSelectButtonClicked,
            Qt::UniqueConnection);
    }
};

template<typename F>
class UserPickerWidget: public ResourcePickerWidgetBase<F, QnSelectUsersButton>
{
public:
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectUsersButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedUsers = m_field->ids();

        ui::SubjectSelectionDialog dialog(this);
        dialog.setAllUsers(m_field->acceptAll());
        dialog.setCheckedSubjects(selectedUsers);
        if (dialog.exec() == QDialog::Rejected)
            return;

        m_field->setAcceptAll(dialog.allUsers());
        m_field->setIds(dialog.totalCheckedUsers());

        updateUi();
    }

    void updateUi() override
    {
        auto resourceList =
            resourcePool()->template getResourcesByIds<QnUserResource>(m_field->ids());

        if (m_field->acceptAll())
            m_selectButton->selectAll();
        else
            m_selectButton->selectUsers(resourceList);

        if (resourceList.isEmpty() && !m_field->acceptAll())
        {
            m_alertLabel->setText(ResourcePickerWidgetStrings::selectUser());
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }
};

using SourceUserPicker = UserPickerWidget<vms::rules::SourceUserField>;
using TargetUserPicker = UserPickerWidget<vms::rules::TargetUserField>;

template<typename F>
class ServerPickerWidget: public ResourcePickerWidgetBase<F, QnSelectServersButton>
{
public:
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedServers = m_field->ids();

        const auto serverFilter = [](const QnMediaServerResourcePtr&){ return true; };
        ServerSelectionDialog::selectServers(selectedServers, serverFilter, QString{}, this);

        m_field->setIds(selectedServers);

        updateUi();
    }

    void updateUi() override
    {
        auto resourceList =
            resourcePool()->template getResourcesByIds<QnMediaServerResource>(m_field->ids());

        m_selectButton->selectServers(resourceList);

        if (resourceList.isEmpty())
        {
            m_alertLabel->setText(ResourcePickerWidgetStrings::selectServer());
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }
};

using ServerPicker = ServerPickerWidget<vms::rules::SourceServerField>;

template<typename F>
class CameraPickerWidget: public ResourcePickerWidgetBase<F, QnSelectDevicesButton>
{
public:
    using ResourcePickerWidgetBase<F, QnSelectDevicesButton>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectDevicesButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectDevicesButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();

        CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
            selectedCameras,
            this);

        m_field->setIds(selectedCameras);

        updateUi();
    }

    void updateUi() override
    {
        auto resourceList =
            resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(m_field->ids());

        m_selectButton->selectDevices(resourceList);

        if (resourceList.isEmpty())
        {
            const auto allCameras = resourcePool()->getAllCameras();
            const auto deviceType
                = QnDeviceDependentStrings::calculateDeviceType(resourcePool(), allCameras);

            m_alertLabel->setText(ResourcePickerWidgetStrings::selectDevice(deviceType));
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }
};

using SourceCameraPicker = CameraPickerWidget<vms::rules::SourceCameraField>;

class TargetCameraPicker: public CameraPickerWidget<vms::rules::TargetDeviceField>
{
public:
    explicit TargetCameraPicker(SystemContext* context, QWidget* parent = nullptr):
        CameraPickerWidget<vms::rules::TargetDeviceField>(context, parent)
    {
        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;
        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString());

        contentLayout->addWidget(m_checkBox);
    }

protected:
    void updateUi() override
    {
        CameraPickerWidget<vms::rules::TargetDeviceField>::updateUi();

        QSignalBlocker sblocker{m_checkBox};
        m_checkBox->setChecked(m_field->useSource());
    }

    void onFieldsSet() override
    {
        CameraPickerWidget<vms::rules::TargetDeviceField>::onFieldsSet();

        connect(m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &TargetCameraPicker::onStateChanged,
            Qt::UniqueConnection);
    }

private:
    QCheckBox* m_checkBox = nullptr;

    void onStateChanged()
    {
        m_field->setUseSource(m_checkBox->isChecked());
    }
};

} // namespace nx::vms::client::desktop::rules
