// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>

#include "../params_widgets/params_widget.h"
#include "../utils/strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename Policy>
class CameraPickerWidgetBase: public ResourcePickerWidgetBase<F>
{
public:
    using base = ResourcePickerWidgetBase<F>;

    CameraPickerWidgetBase(F* field, SystemContext* context, ParamsWidget* parent):
        ResourcePickerWidgetBase<F>{field, context, parent}
    {
    }
};

template<typename Policy>
class SourceCameraPicker: public CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>
{
public:
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::CameraPickerWidgetBase;

protected:
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::m_field;
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::systemContext;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();

        if (CameraSelectionDialog::selectCameras<Policy>(systemContext(), selectedCameras, this))
        {
            m_field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
            m_field->setIds(selectedCameras);
        }

        CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::onSelectButtonClicked();
    }
};

template<typename Policy>
class TargetCameraPicker: public CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>
{
public:
    TargetCameraPicker(
        vms::rules::TargetDeviceField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>(field, context, parent)
    {
        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;
        m_checkBox->setText(Strings::useSourceCameraString(parentParamsWidget()->descriptor()->id));

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &TargetCameraPicker<Policy>::onStateChanged);
    }

protected:
    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();

        if (CameraSelectionDialog::selectCameras<Policy>(systemContext(), selectedCameras, this))
        {
            m_field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
            m_field->setIds(selectedCameras);
        }

        CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::onSelectButtonClicked();
    }

    void updateUi() override
    {
        CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::updateUi();

        m_checkBox->setEnabled(
            vms::rules::hasSourceCamera(parentParamsWidget()->eventDescriptor().value()));

        QSignalBlocker blocker{m_checkBox};
        m_checkBox->setChecked(m_field->useSource());
    }

private:
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_field;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_contentWidget;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::connect;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::systemContext;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::parentParamsWidget;

    QCheckBox* m_checkBox{nullptr};

    void onStateChanged()
    {
        m_field->setUseSource(m_checkBox->isChecked());
    }
};

} // namespace nx::vms::client::desktop::rules
