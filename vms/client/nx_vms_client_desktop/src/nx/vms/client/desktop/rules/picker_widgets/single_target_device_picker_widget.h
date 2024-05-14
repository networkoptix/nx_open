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
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/field.h>

#include "picker_widget_strings.h"
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
        m_checkBox->setText(ResourcePickerWidgetStrings::useEventSourceString(
            parentParamsWidget()->descriptor()->id));

        contentLayout->addWidget(m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &SingleTargetDevicePicker::onStateChanged);
    }

private:
    QCheckBox* m_checkBox{nullptr};

    void onSelectButtonClicked() override
    {
        auto selectedCameras = UuidSet{m_field->id()};

        if (!CameraSelectionDialog::selectCameras<Policy>(selectedCameras, this))
            return;

        m_field->setId(*selectedCameras.begin());
        m_field->setUseSource(false);
    }

    void updateUi() override
    {
        const auto canUseSource = Policy::canUseSourceCamera();
        const auto hasSource = vms::rules::hasSourceCamera(*parentParamsWidget()->eventDescriptor());
        const auto useSource = m_field->useSource();

        if (canUseSource && useSource && !hasSource)
        {
            m_field->setUseSource(false);
            return;
        }

        if (canUseSource && useSource)
        {
            m_selectButton->setText(CameraPickerStrings::sourceCameraString());
            m_selectButton->setIcon(core::Skin::maximumSizePixmap(
                qnResIconCache->icon(QnResourceIconCache::Camera),
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }
        else
        {
            const auto camera =
                resourcePool()->template getResourceById<QnVirtualCameraResource>(m_field->id());

            m_selectButton->setText(Policy::getText({camera}));
            m_selectButton->setIcon(core::Skin::maximumSizePixmap(
                icon(camera),
                QIcon::Selected,
                QIcon::Off,
                /*correctDevicePixelRatio*/ false));
        }

        if (canUseSource)
        {
            m_checkBox->setEnabled(hasSource);
            m_selectButton->setEnabled(!useSource);

            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(useSource);
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

    QIcon icon(const QnVirtualCameraResourcePtr& camera)
    {
        if (!camera)
            return qnSkin->icon(core::kAlertIcon);

        const auto layoutIdsField =
            getActionField<vms::rules::TargetLayoutField>(vms::rules::utils::kLayoutIdsFieldName);
        if (layoutIdsField)
        {
            const auto layoutIds = layoutIdsField->value();
            const auto layouts =
                resourcePool()->template getResourcesByIds<QnLayoutResource>(layoutIds);
            const auto isCameraExistsOnLayouts = std::all_of(
                layouts.cbegin(),
                layouts.cend(),
                [camera](const auto& layout)
                {
                    return resourceBelongsToLayout(camera, layout);
                });

            return isCameraExistsOnLayouts
                ? qnResIconCache->icon(camera)
                : qnResIconCache->icon(
                    QnResourceIconCache::Camera | QnResourceIconCache::Incompatible);
        }

        return qnResIconCache->icon(camera);
    }
};

} // namespace nx::vms::client::desktop::rules
