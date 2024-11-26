// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>

#include "../params_widgets/params_widget.h"
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
class TargetCameraPicker: public CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>
{
public:
    TargetCameraPicker(
        vms::rules::TargetDevicesField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>(field, context, parent)
    {
    }

protected:
    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();
        bool useSource = m_field->useSource();

        if (hasSourceCamera(getEventDescriptor().value()))
        {
            if (CameraSelectionDialog::selectCameras<Policy>(
                systemContext(),
                selectedCameras,
                useSource,
                this))
            {
                m_field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
                m_field->setIds(selectedCameras);
                m_field->setUseSource(useSource);
            }
        }
        else
        {
            if (CameraSelectionDialog::selectCameras<Policy>(
                systemContext(),
                selectedCameras,
                this))
            {
                m_field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
                m_field->setIds(selectedCameras);
                m_field->setUseSource(false);
            }
        }

        CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>::onSelectButtonClicked();
    }

private:
    using CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>::m_field;
    using CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>::systemContext;
    using CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>::parentParamsWidget;
    using CameraPickerWidgetBase<vms::rules::TargetDevicesField, Policy>::getEventDescriptor;
};

} // namespace nx::vms::client::desktop::rules
