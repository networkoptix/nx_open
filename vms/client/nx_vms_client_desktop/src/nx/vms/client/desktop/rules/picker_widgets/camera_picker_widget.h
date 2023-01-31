// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <ui/widgets/select_resources_button.h>

#include "../utils/resource_selection_policy.h"
#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class CameraPickerWidgetBase: public ResourcePickerWidgetBase<F, QnSelectDevicesButton>
{
public:
    explicit CameraPickerWidgetBase(SystemContext* context, CommonParamsWidget* parent)
        : ResourcePickerWidgetBase<F, QnSelectDevicesButton>(context, parent)
    {
    }

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectDevicesButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectDevicesButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();

        CameraSelectionDialog::selectCameras<MultiChoicePolicy>(
            selectedCameras,
            this);

        m_field->setIds(selectedCameras);

        updateValue();
    }

    void updateValue() override
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

class SourceCameraPicker: public CameraPickerWidgetBase<vms::rules::SourceCameraField>
{
public:
    using CameraPickerWidgetBase<vms::rules::SourceCameraField>::CameraPickerWidgetBase;
};

class TargetCameraPicker: public CameraPickerWidgetBase<vms::rules::TargetDeviceField>
{
public:
    TargetCameraPicker(SystemContext* context, CommonParamsWidget* parent):
        CameraPickerWidgetBase<vms::rules::TargetDeviceField>(context, parent)
    {
        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;
        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString());

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &TargetCameraPicker::onStateChanged);
    }

protected:
    void updateValue() override
    {
        CameraPickerWidgetBase<vms::rules::TargetDeviceField>::updateValue();

        QSignalBlocker sblocker{m_checkBox};
        m_checkBox->setChecked(m_field->useSource());
    }

private:
    QCheckBox* m_checkBox{nullptr};

    void onStateChanged()
    {
        m_field->setUseSource(m_checkBox->isChecked());
    }
};

class SingleTargetCameraPicker:
    public ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField, QnSelectDevicesButton>
{
public:
    SingleTargetCameraPicker(SystemContext* context, CommonParamsWidget* parent):
        ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField, QnSelectDevicesButton>(context, parent)
    {
        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;
        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString());

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &SingleTargetCameraPicker::onStateChanged);
    }

private:
    QCheckBox* m_checkBox{nullptr};

    void onSelectButtonClicked() override
    {
        auto selectedCameras = QnUuidSet{m_field->id()};

        CameraSelectionDialog::selectCameras<SingleChoicePolicy>(selectedCameras, this);

        m_field->setId(*selectedCameras.begin());

        updateValue();
    }

    void updateValue() override
    {
        auto resourceList =
            resourcePool()->getResourcesByIds<QnVirtualCameraResource>(QnUuidSet{m_field->id()});

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

        {
            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(m_field->useSource());
        }

        updateUi();
    }

    void onStateChanged()
    {
        m_field->setUseSource(m_checkBox->isChecked());
        updateUi();
    }

    void updateUi()
    {
        if (m_checkBox->isChecked())
        {
            m_label->setVisible(false);
            m_selectResourceWidget->setVisible(false);
        }
        else
        {
            m_label->setVisible(true);
            m_selectResourceWidget->setVisible(true);
        }
    }
};

} // namespace nx::vms::client::desktop::rules
