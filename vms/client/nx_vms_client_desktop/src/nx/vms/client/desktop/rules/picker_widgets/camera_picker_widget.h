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

#include "../params_widgets/common_params_widget.h"
#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename Policy>
class CameraPickerWidgetBase: public ResourcePickerWidgetBase<F>
{
public:
    using base = ResourcePickerWidgetBase<F>;
    using ResourcePickerWidgetBase<F>::ResourcePickerWidgetBase;

protected:
    BASE_COMMON_USINGS
    using ResourcePickerWidgetBase<F>::updateUi;

    void onSelectButtonClicked() override
    {
        auto field = theField();
        auto selectedCameras = field->ids();

        if (CameraSelectionDialog::selectCameras<Policy>(selectedCameras, this))
        {
            field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
            field->setIds(selectedCameras);
        }
    }
};

template<typename Policy>
class SourceCameraPicker: public CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>
{
public:
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::CameraPickerWidgetBase;

protected:
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::m_selectButton;
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::theField;
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::resourcePool;

    void updateUi() override
    {
        auto resources =
            resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(theField()->ids());

        m_selectButton->setText(Policy::getText(resources, /*detailed*/ true));
        m_selectButton->setIcon(core::Skin::maximumSizePixmap(resources.size() == 1
                ? qnResIconCache->icon(resources.first())
                : qnResIconCache->icon(QnResourceIconCache::Camera),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false));
    }
};

template<typename Policy>
class TargetCameraPicker: public CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>
{
public:
    TargetCameraPicker(SystemContext* context, CommonParamsWidget* parent):
        CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>(context, parent)
    {
        auto contentLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());

        m_checkBox = new QCheckBox;

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &TargetCameraPicker<Policy>::onStateChanged);
    }

protected:
    void onDescriptorSet() override
    {
        CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::onDescriptorSet();

        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString(
            parentParamsWidget()->descriptor().id));
    }

    void updateUi() override
    {
        auto field = theField();
        const auto useSource = field->useSource();
        const auto hasSource = vms::rules::hasSourceCamera(*parentParamsWidget()->eventDescriptor());

        if (useSource && !hasSource)
        {
            field->setUseSource(false);
            return;
        }

        const auto resources =
            resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(field->ids());

        if (useSource)
        {
            m_selectButton->setText(CameraPickerStrings::sourceCameraString(resources.size()));
            m_selectButton->setIcon(core::Skin::maximumSizePixmap(
                qnResIconCache->icon(QnResourceIconCache::Camera),
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }
        else
        {
            m_selectButton->setText(Policy::getText(resources, /*detailed*/ true));

            QIcon icon;
            if (resources.empty())
                icon = qnSkin->icon("tree/buggy.png");
            else if (resources.size() == 1)
                icon = qnResIconCache->icon(resources.first());
            else
                icon = qnResIconCache->icon(QnResourceIconCache::Cameras);

            m_selectButton->setIcon(core::Skin::maximumSizePixmap(
                icon,
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }

        m_checkBox->setEnabled(hasSource);

        QSignalBlocker blocker{m_checkBox};
        m_checkBox->setChecked(useSource);
    }

private:
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::theField;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_contentWidget;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_selectButton;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::connect;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::resourcePool;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::parentParamsWidget;

    QCheckBox* m_checkBox{nullptr};

    void onStateChanged()
    {
        theField()->setUseSource(m_checkBox->isChecked());
    }
};

} // namespace nx::vms::client::desktop::rules
