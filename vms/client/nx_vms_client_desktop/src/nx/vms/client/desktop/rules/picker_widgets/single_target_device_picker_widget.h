// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>
#include <nx/vms/rules/camera_validation_policy.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>

#include "../utils/strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename Policy>
class SingleTargetDevicePicker:
    public ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>
{
public:
    SingleTargetDevicePicker(
        vms::rules::TargetSingleDeviceField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>(field, context, parent)
    {
        if (!Policy::canUseSourceCamera())
            return;

        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;
        m_checkBox->setText(Strings::useSourceCameraString(parentParamsWidget()->descriptor()->id));

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &SingleTargetDevicePicker::onStateChanged);
    }

private:
    QCheckBox* m_checkBox{nullptr};
    Policy policy;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = UuidSet{m_field->id()};

        preparePolicy();
        if (!CameraSelectionDialog::selectCameras(systemContext(), selectedCameras, policy, this)
            || selectedCameras.empty())
        {
            return;
        }

        m_field->setId(*selectedCameras.begin());
        m_field->setUseSource(false);
    }

    void preparePolicy();

    void updateUi() override
    {
        ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>::updateUi();

        if (Policy::canUseSourceCamera())
        {
            m_checkBox->setEnabled(
                vms::rules::hasSourceCamera(*parentParamsWidget()->eventDescriptor()));
            m_selectButton->setEnabled(!m_field->useSource());

            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(m_field->useSource());
        }
    }

    void onStateChanged()
    {
        if (m_checkBox->isChecked())
        {
            m_field->setUseSource(true);
            m_field->setId({});
        }
        else
        {
            m_field->setUseSource(false);
        }
    }
};

template<typename Policy>
void SingleTargetDevicePicker<Policy>::preparePolicy()
{
}

template<>
void SingleTargetDevicePicker<QnFullscreenCameraPolicy>::preparePolicy()
{
    const auto layoutField =
        getActionField<vms::rules::TargetLayoutField>(vms::rules::utils::kLayoutIdsFieldName);
    if (!NX_ASSERT(layoutField))
        return;

    const auto layouts = resourcePool()->getResourcesByIds<QnLayoutResource>(layoutField->value());
    policy.setLayouts(layouts);
}

} // namespace nx::vms::client::desktop::rules
