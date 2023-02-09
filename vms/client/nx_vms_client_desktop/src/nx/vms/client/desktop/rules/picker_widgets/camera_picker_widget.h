// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/field.h>

#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename Policy>
class CameraPickerWidgetBase: public ResourcePickerWidgetBase<F>
{
public:
    using ResourcePickerWidgetBase<F>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F>::updateUi;

    void onSelectButtonClicked() override
    {
        auto selectedCameras = m_field->ids();

        if (CameraSelectionDialog::selectCameras<Policy>(selectedCameras, this))
        {
            m_field->setAcceptAll(Policy::emptyListIsValid() && selectedCameras.empty());
            m_field->setIds(selectedCameras);
            updateUi();
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
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::m_field;
    using CameraPickerWidgetBase<vms::rules::SourceCameraField, Policy>::resourcePool;

    void updateUi() override
    {
        auto resources =
            resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(m_field->ids());

        m_selectButton->setText(Policy::getText(resources, /*detailed*/ true));
        m_selectButton->setIcon(Skin::maximumSizePixmap(resources.size() == 1
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
        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString());

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &TargetCameraPicker<Policy>::onStateChanged);
    }

protected:
    void updateUi() override
    {
        auto resources =
            resourcePool()->template getResourcesByIds<QnVirtualCameraResource>(m_field->ids());

        if (m_field->useSource())
        {
            m_selectButton->setText(CameraPickerStrings::sourceCameraString(resources.size()));
            m_selectButton->setIcon(Skin::maximumSizePixmap(
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

            m_selectButton->setIcon(Skin::maximumSizePixmap(
                icon,
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }

        QSignalBlocker blocker{m_checkBox};
        m_checkBox->setChecked(m_field->useSource());
    }

private:
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_field;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_contentWidget;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::m_selectButton;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::connect;
    using CameraPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::resourcePool;

    QCheckBox* m_checkBox{nullptr};

    void onStateChanged()
    {
        m_field->setUseSource(m_checkBox->isChecked());
        updateUi();
    }
};

} // namespace nx::vms::client::desktop::rules
